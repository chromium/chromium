// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.signin.AccountCapabilitiesConstants.IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccountManagerDelegate.CapabilityResponse;
import org.chromium.components.signin.ConnectionRetry.AuthTask;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/** AccountManagerFacade wraps our access of AccountManager in Android. */
@NullMarked
public class AccountManagerFacadeImpl implements AccountManagerFacade {
    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a USM account.
     */
    @VisibleForTesting public static final String FEATURE_IS_USM_ACCOUNT_KEY = "service_usm";

    /**
     * The maximum amount of acceptable retries (for a total of MAXIMUM_RETRIES+1 attempts). *
     *
     * <p>WARNING: This is tied to the number of buckets of a UMA histogram and should therefore not
     * exceed 100.
     */
    @VisibleForTesting public static final int MAXIMUM_RETRIES = 5;

    // Prefix used to define the capability name for querying Identity services. This
    // prefix is not required for Android queries to GmsCore.
    private static final String ACCOUNT_CAPABILITY_NAME_PREFIX = "accountcapabilities/";

    // Time, in milliseconds, between two attempts to fetch the accounts.
    private static final long GET_ACCOUNTS_BACKOFF_DELAY = 1000L;

    private static final String TAG = "AccountManager";

    private final AccountManagerDelegate mDelegate;

    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();

    private final AtomicReference<List<Account>> mAllAccounts = new AtomicReference<>();
    private final AtomicReference<List<PatternMatcher>> mAccountRestrictionPatterns =
            new AtomicReference<>();

    // Deprecated in favor of `mAccountsPromise`, to be removed after migrating all affected calls.
    private Promise<List<CoreAccountInfo>> mCoreAccountInfosPromise = new Promise<>();

    private Promise<List<AccountInfo>> mAccountsPromise = new Promise<>();

    private @Nullable AsyncTask<@Nullable List<GaiaId>> mFetchGaiaIdsTask;

    private int mNumberOfRetries;
    private boolean mDidAccountFetchSucceed;

    private int mPendingTokenRequests;
    private @Nullable Runnable mTokenRequestsCompletedCallback;

    private boolean mDisallowTokenRequestsForTesting;

    /**
     * @param delegate the AccountManagerDelegate to use as a backend
     */
    public AccountManagerFacadeImpl(AccountManagerDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
        mDelegate.attachAccountsChangeObserver(this::onAccountsUpdated);
        new AccountRestrictionPatternReceiver(this::onAccountRestrictionPatternsUpdated);

        getAccounts()
                .then(
                        accounts -> {
                            RecordHistogram.recordExactLinearHistogram(
                                    "Signin.AndroidNumberOfDeviceAccounts", accounts.size(), 50);
                        });
        onAccountsUpdated();
    }

    /**
     * Adds an observer to receive accounts change notifications.
     * @param observer the observer to add.
     */
    @Override
    public void addObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        boolean success = mObservers.addObserver(observer);
        assert success : "Observer already added!";
    }

    /**
     * Removes an observer that was previously added using {@link #addObserver}.
     * @param observer the observer to remove.
     */
    @Override
    public void removeObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        boolean success = mObservers.removeObserver(observer);
        assert success : "Can't find observer";
    }

    @MainThread
    @Override
    public Promise<List<CoreAccountInfo>> getCoreAccountInfos() {
        ThreadUtils.assertOnUiThread();
        return mCoreAccountInfosPromise;
    }

    @MainThread
    @Override
    public Promise<List<AccountInfo>> getAccounts() {
        ThreadUtils.assertOnUiThread();
        return mAccountsPromise;
    }

    @MainThread
    @Override
    public void getAccessToken(
            CoreAccountInfo coreAccountInfo, String scope, GetAccessTokenCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert coreAccountInfo != null;
        assert scope != null;

        if (mDisallowTokenRequestsForTesting) {
            callback.onGetTokenFailure(
                    new GoogleServiceAuthError(GoogleServiceAuthErrorState.REQUEST_CANCELED));
            return;
        }

        pendingRequestStarted();
        ConnectionRetry.runAuthTask(
                new AuthTask() {
                    @Override
                    public AccessTokenData run() throws AuthException {
                        return mDelegate.getAccessToken(
                                CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo), scope);
                    }

                    @Override
                    public void onSuccess(@Nullable AccessTokenData token) {
                        assert token != null : "AccessTokenData must not be null on success.";
                        callback.onGetTokenSuccess(token);
                        pendingRequestFinished();
                    }

                    @Override
                    public void onFailure(GoogleServiceAuthError authError) {
                        callback.onGetTokenFailure(authError);
                        pendingRequestFinished();
                    }
                });
    }

    private void pendingRequestStarted() {
        ThreadUtils.assertOnUiThread();
        mPendingTokenRequests++;
    }

    private void pendingRequestFinished() {
        ThreadUtils.assertOnUiThread();
        mPendingTokenRequests--;
        assert mPendingTokenRequests >= 0;
        if (mPendingTokenRequests == 0 && mTokenRequestsCompletedCallback != null) {
            Runnable callback = mTokenRequestsCompletedCallback;
            mTokenRequestsCompletedCallback = null;
            callback.run();
        }
    }

    @Override
    public void invalidateAccessToken(String accessToken, @Nullable Runnable completedRunnable) {
        ThreadUtils.assertOnUiThread();
        if (TextUtils.isEmpty(accessToken) || mDisallowTokenRequestsForTesting) {
            // TODO(https://crbug.com/366403142): Replace isEmpty check with an exception.
            if (completedRunnable != null) {
                completedRunnable.run();
            }
            return;
        }
        ConnectionRetry.runAuthTask(
                new AuthTask() {
                    @Override
                    public @Nullable AccessTokenData run() throws AuthException {
                        mDelegate.invalidateAccessToken(accessToken);
                        return null;
                    }

                    @Override
                    public void onSuccess(@Nullable AccessTokenData ignored) {
                        if (completedRunnable != null) {
                            completedRunnable.run();
                        }
                    }

                    @Override
                    public void onFailure(GoogleServiceAuthError ignored) {
                        if (completedRunnable != null) {
                            completedRunnable.run();
                        }
                    }
                });
    }

    @Override
    public void waitForPendingTokenRequestsToComplete(Runnable requestsCompletedCallback) {
        ThreadUtils.assertOnUiThread();
        assert mTokenRequestsCompletedCallback == null;
        if (mPendingTokenRequests == 0) {
            requestsCompletedCallback.run();
            return;
        }
        // The callback will be invoked when the all pending token requests are finished.
        mTokenRequestsCompletedCallback = requestsCompletedCallback;
    }

    @Override
    public void checkChildAccountStatus(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener) {
        ThreadUtils.assertOnUiThread();
        new AsyncTask<Boolean>() {
            @Override
            public Boolean doInBackground() {
                Account account = CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo);
                return mDelegate.hasFeature(account, FEATURE_IS_USM_ACCOUNT_KEY);
            }

            @Override
            protected void onPostExecute(Boolean isChild) {
                // TODO(crbug.com/40201126): rework this interface to avoid passing a null account.
                listener.onStatusReady(isChild, isChild ? coreAccountInfo : null);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void checkIsSubjectToParentalControls(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener) {
        ThreadUtils.assertOnUiThread();
        new AsyncTask<Boolean>() {
            @Override
            public Boolean doInBackground() {
                Account account = CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo);
                @CapabilityResponse
                int capability =
                        mDelegate.hasCapability(
                                account,
                                getAndroidCapabilityName(
                                        IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME));
                return capability == CapabilityResponse.YES;
            }

            @Override
            protected void onPostExecute(Boolean isSubjectToParentalControls) {
                // TODO(crbug.com/40201126): rework this interface to avoid passing a null account.
                listener.onStatusReady(
                        isSubjectToParentalControls,
                        isSubjectToParentalControls ? coreAccountInfo : null);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * @param account The account used to look up capabilities.
     * @return Set of supported account capability values.
     */
    @Override
    public Promise<AccountCapabilities> getAccountCapabilities(CoreAccountInfo coreAccountInfo) {
        ThreadUtils.assertOnUiThread();
        Promise<AccountCapabilities> accountCapabilitiesPromise = new Promise<>();
        new AsyncTask<AccountCapabilities>() {
            @Override
            public AccountCapabilities doInBackground() {
                Map<String, Integer> capabilitiesResponse = new HashMap<>();
                for (String capabilityName :
                        AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
                    @CapabilityResponse
                    int capability =
                            mDelegate.hasCapability(
                                    CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo),
                                    getAndroidCapabilityName(capabilityName));
                    capabilitiesResponse.put(capabilityName, capability);
                }
                return AccountCapabilities.parseFromCapabilitiesResponse(capabilitiesResponse);
            }

            @Override
            protected void onPostExecute(AccountCapabilities result) {
                accountCapabilitiesPromise.fulfill(result);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        return accountCapabilitiesPromise;
    }

    /**
     * Creates an intent that will ask the user to add a new account to the device. See {@link
     * AccountManager#addAccount} for details.
     *
     * @param callback The callback to get the created intent. Will be invoked on the main thread.
     *     If there is an issue while creating the intent, callback will receive null.
     */
    @Override
    public void createAddAccountIntent(Callback<@Nullable Intent> callback) {
        RecordUserAction.record("Signin_AddAccountToDevice");
        mDelegate.createAddAccountIntent(callback);
    }

    /**
     * Asks the user to enter a new password for an account, updating the saved credentials for the
     * account.
     */
    @Override
    public void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback) {
        mDelegate.updateCredentials(account, activity, callback);
    }

    @Override
    public void confirmCredentials(
            Account account, @Nullable Activity activity, Callback<@Nullable Bundle> callback) {
        mDelegate.confirmCredentials(account, activity, callback);
    }

    @Override
    public boolean didAccountFetchSucceed() {
        return mDidAccountFetchSucceed;
    }

    /**
     * Fetches gaia ids, creates account objects and updates {@link #mCoreAccountInfosPromise} and
     * {@link #mAccountsPromise}.
     */
    @MainThread
    private void fetchGaiaIdsAndUpdateCoreAccountInfos() {
        ThreadUtils.assertOnUiThread();
        if (mFetchGaiaIdsTask != null) {
            // Cancel previous fetch task as it is obsolete now.
            mFetchGaiaIdsTask.cancel(true);
            mFetchGaiaIdsTask = null;
        }

        mFetchGaiaIdsTask = new GetAccountAsyncTask(getFilteredAccountEmails());
        mFetchGaiaIdsTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private void onAccountsUpdated() {
        ThreadUtils.assertOnUiThread();
        new AsyncTask<@Nullable List<Account>>() {
            @Override
            protected @Nullable List<Account> doInBackground() {
                try {
                    return Collections.unmodifiableList(
                            Arrays.asList(mDelegate.getAccountsSynchronous()));
                } catch (AccountManagerDelegateException delegateException) {
                    Log.e(TAG, "Error fetching accounts from the delegate.", delegateException);
                    return null;
                }
            }

            @Override
            protected void onPostExecute(@Nullable List<Account> allAccounts) {
                mDidAccountFetchSucceed = true;
                if (allAccounts == null) {
                    mDidAccountFetchSucceed = false;
                    if (shouldRetry()) {
                        // Wait for a fixed amount of time then try to fetch the accounts again.
                        PostTask.postDelayedTask(
                                TaskTraits.UI_USER_VISIBLE,
                                () -> {
                                    onAccountsUpdated();
                                },
                                GET_ACCOUNTS_BACKOFF_DELAY);
                        return;
                    } else {
                        // We shouldn't wait indefinitely for the account fetching to succeed, at it
                        // might block certain features. Fall back to an empty list to allow the
                        // user to proceed.
                        allAccounts = mAllAccounts.get() == null ? List.of() : mAllAccounts.get();
                    }
                }
                if (mNumberOfRetries != 0) {
                    RecordHistogram.recordBooleanHistogram(
                            "Signin.GetAccountsBackoffSuccess", mDidAccountFetchSucceed);
                    if (mDidAccountFetchSucceed) {
                        RecordHistogram.recordExactLinearHistogram(
                                "Signin.GetAccountsBackoffRetries",
                                mNumberOfRetries,
                                MAXIMUM_RETRIES + 1);
                    }
                }
                mNumberOfRetries = 0;
                mAllAccounts.set(allAccounts);
                updateAccounts();
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private boolean shouldRetry() {
        if (mNumberOfRetries < MAXIMUM_RETRIES) {
            mNumberOfRetries += 1;
            return true;
        }
        return false;
    }

    private void onAccountRestrictionPatternsUpdated(List<PatternMatcher> patternMatchers) {
        mAccountRestrictionPatterns.set(patternMatchers);
        updateAccounts();
    }

    @MainThread
    private void updateAccounts() {
        if (mAllAccounts.get() == null || mAccountRestrictionPatterns.get() == null) {
            return;
        }
        fetchGaiaIdsAndUpdateCoreAccountInfos();
    }

    private List<String> getFilteredAccountEmails() {
        List<String> ret = new ArrayList<>();
        List<PatternMatcher> restrictions = mAccountRestrictionPatterns.get();
        assumeNonNull(restrictions);
        for (Account account : assumeNonNull(mAllAccounts.get())) {
            String name = account.name;
            boolean matches = restrictions.isEmpty();
            for (PatternMatcher matcher : restrictions) {
                if (matches) {
                    break;
                }
                matches = matcher.matches(name);
            }
            if (matches) {
                ret.add(name);
            }
        }
        return ret;
    }

    /**
     * @param capabilityName the name of the capability used to query Identity services.
     * @return the name of the capability used to query GmsCore.
     */
    static String getAndroidCapabilityName(String capabilityName) {
        if (capabilityName.startsWith(ACCOUNT_CAPABILITY_NAME_PREFIX)) {
            return capabilityName.substring(ACCOUNT_CAPABILITY_NAME_PREFIX.length());
        }
        return capabilityName;
    }

    public void resetAccountsForTesting() {
        mCoreAccountInfosPromise = new Promise<>();
        mAccountsPromise = new Promise<>();
        mAllAccounts.set(null);
        updateAccounts();
    }

    @Override
    public void disallowTokenRequestsForTesting() {
        ThreadUtils.assertOnUiThread();
        mDisallowTokenRequestsForTesting = true;
    }

    private class GetAccountAsyncTask extends AsyncTask<@Nullable List<GaiaId>> {
        private final List<String> mEmails;

        GetAccountAsyncTask(List<String> emails) {
            mEmails = emails;
        }

        @Override
        public @Nullable List<GaiaId> doInBackground() {
            final long seedingStartTime = SystemClock.elapsedRealtime();
            List<GaiaId> gaiaIds = new ArrayList<>();
            for (String email : mEmails) {
                if (isCancelled()) {
                    return null;
                }
                final GaiaId gaiaId = mDelegate.getAccountGaiaId(email);
                if (gaiaId == null) {
                    // TODO(crbug.com/40275966): Add metrics to check how often we get a
                    // null gaiaId.
                    return null;
                }
                gaiaIds.add(gaiaId);
            }
            RecordHistogram.recordTimesHistogram(
                    "Signin.AndroidGetAccountIdsTime",
                    SystemClock.elapsedRealtime() - seedingStartTime);
            return gaiaIds;
        }

        @Override
        public void onPostExecute(@Nullable List<GaiaId> gaiaIds) {
            mFetchGaiaIdsTask = null;
            if (gaiaIds == null) {
                fetchGaiaIdsAndUpdateCoreAccountInfos();
                return;
            }
            List<CoreAccountInfo> coreAccountInfos = new ArrayList<>();
            List<AccountInfo> accounts = new ArrayList<>();
            for (int index = 0; index < mEmails.size(); index++) {
                String email = mEmails.get(index);
                GaiaId gaiaId = gaiaIds.get(index);
                coreAccountInfos.add(CoreAccountInfo.createFromEmailAndGaiaId(email, gaiaId));
                accounts.add(new AccountInfo.Builder(email, gaiaId).build());
            }
            assert mCoreAccountInfosPromise.isFulfilled() == mAccountsPromise.isFulfilled();
            if (mCoreAccountInfosPromise.isFulfilled()) {
                mCoreAccountInfosPromise = Promise.fulfilled(coreAccountInfos);
                mAccountsPromise = Promise.fulfilled(accounts);
            } else {
                mCoreAccountInfosPromise.fulfill(coreAccountInfos);
                mAccountsPromise.fulfill(accounts);
            }
            for (AccountsChangeObserver observer : mObservers) {
                observer.onCoreAccountInfosChanged();
            }
        }
    }
}
