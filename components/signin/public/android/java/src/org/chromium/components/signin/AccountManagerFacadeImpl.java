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
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicReference;

/** AccountManagerFacade wraps our access of AccountManager in Android. */
@NullMarked
public class AccountManagerFacadeImpl implements AccountManagerFacade {
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

    private static final String OAUTH2_SCOPE_PREFIX = "oauth2:";

    private static final String TAG = "AccountManager";

    private final AccountManagerDelegate mDelegate;

    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();

    private final AtomicReference<@Nullable List<Account>> mAllAccounts = new AtomicReference<>();
    private final AtomicReference<@Nullable List<PlatformAccount>> mAllPlatformAccounts =
            new AtomicReference<>();
    private final AtomicReference<@Nullable List<PatternMatcher>> mAccountRestrictionPatterns =
            new AtomicReference<>();
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

        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            mDelegate.attachAccountsChangeObserver(this::onPlatformAccountsUpdated);
            onPlatformAccountsUpdated();
        } else {
            mDelegate.attachAccountsChangeObserver(this::onAccountsUpdated);
            onAccountsUpdated();
        }
        new AccountRestrictionPatternReceiver(this::onAccountRestrictionPatternsUpdated);

        getAccounts()
                .then(
                        accounts -> {
                            RecordHistogram.recordExactLinearHistogram(
                                    "Signin.AndroidNumberOfDeviceAccounts", accounts.size(), 50);
                        });
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

        if (!SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            String oauth2Scope = OAUTH2_SCOPE_PREFIX + scope;
            ConnectionRetry.runAuthTask(
                    new AuthTask() {
                        @Override
                        public AccessTokenData run() throws AuthException {
                            return mDelegate.getAccessToken(
                                    CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo),
                                    oauth2Scope);
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
            return;
        }

        getAccounts()
                .then(
                        unused -> {
                            getAccessTokenHelper(coreAccountInfo, scope, callback);
                        });
    }

    private void getAccessTokenHelper(
            CoreAccountInfo coreAccountInfo, String scope, GetAccessTokenCallback callback) {
        PlatformAccount platformAccount = getPlatformAccount(coreAccountInfo.getGaiaId());
        if (platformAccount == null) {
            callback.onGetTokenFailure(
                    new GoogleServiceAuthError(GoogleServiceAuthErrorState.USER_NOT_SIGNED_UP));
            pendingRequestFinished();
            return;
        }

        ConnectionRetry.runAuthTask(
                new AuthTask() {
                    @Override
                    public AccessTokenData run() throws AuthException {
                        return mDelegate.getAccessTokenForPlatformAccount(platformAccount, scope);
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
                        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
                            mDelegate.invalidateAccessTokenForPlatformAccount(accessToken);
                            return null;
                        }

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
    public void checkIsSubjectToParentalControls(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener) {
        ThreadUtils.assertOnUiThread();
        if (!SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
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
                    // TODO(crbug.com/40201126): rework this interface to avoid passing a null
                    // account.
                    listener.onStatusReady(
                            isSubjectToParentalControls,
                            isSubjectToParentalControls ? coreAccountInfo : null);
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        // Wait for list of accounts to be available before checking capabilities.
        getAccounts()
                .then(
                        unused -> {
                            checkIsSubjectToParentalControlsHelper(coreAccountInfo, listener);
                        });
    }

    private void checkIsSubjectToParentalControlsHelper(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener) {
        assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
        @Nullable PlatformAccount account = getPlatformAccount(coreAccountInfo.getGaiaId());
        if (account == null) {
            listener.onStatusReady(false, null);
            return;
        }

        new AsyncTask<Boolean>() {
            @Override
            public Boolean doInBackground() {
                @CapabilityResponse
                int capability =
                        mDelegate.fetchCapability(
                                account,
                                getAndroidCapabilityName(
                                        IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME));
                return capability == CapabilityResponse.YES;
            }

            @Override
            protected void onPostExecute(Boolean isSubjectToParentalControls) {
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
        // TODO(crbug.com/436520680): Remove non signin uses of getAccountCapabilities.
        ThreadUtils.assertOnUiThread();

        Promise<AccountCapabilities> accountCapabilitiesPromise = new Promise<>();
        if (!SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
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

        getAccounts()
                .then(
                        unused -> {
                            fetchCapabilitiesHelper(coreAccountInfo, accountCapabilitiesPromise);
                        });
        return accountCapabilitiesPromise;
    }

    private void fetchCapabilitiesHelper(
            CoreAccountInfo coreAccountInfo,
            Promise<AccountCapabilities> accountCapabilitiesPromise) {
        assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();

        @Nullable PlatformAccount account = getPlatformAccount(coreAccountInfo.getGaiaId());
        if (account == null) {
            // if there is no account, the capabilities will be empty.
            return;
        }

        new AsyncTask<AccountCapabilities>() {
            @Override
            public AccountCapabilities doInBackground() {
                Map<String, Integer> capabilitiesResponse = new HashMap<>();
                for (String capabilityName :
                        AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
                    @CapabilityResponse
                    int capability =
                            mDelegate.fetchCapability(
                                    assumeNonNull(account),
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
    }

    @Nullable
    private PlatformAccount getPlatformAccount(GaiaId gaiaId) {
        assert getAccounts().isFulfilled();
        if (mAllPlatformAccounts.get() == null) {
            return null;
        }

        for (PlatformAccount account : assumeNonNull(mAllPlatformAccounts.get())) {
            if (Objects.equals(account.getId(), gaiaId)) {
                return account;
            }
        }

        return null;
    }

    /**
     * Creates an intent that will ask the user to add a new account to the device. See {@link
     * AccountManager#addAccount} for details.
     *
     * @param callback The callback to get the created intent. Will be invoked on the main thread.
     *     If there is an issue while creating the intent, callback will receive null.
     */
    @Override
    public void createAddAccountIntent(
            @Nullable String prefilledEmail, Callback<@Nullable Intent> callback) {
        RecordUserAction.record("Signin_AddAccountToDevice");
        mDelegate.createAddAccountIntent(prefilledEmail, callback);
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

    /** Fetches gaia ids, creates account objects and updates {@link #mAccountsPromise}. */
    @MainThread
    private void fetchGaiaIdsAndUpdateCoreAccountInfos() {
        assert !SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
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
        assert !SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
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

    private void onPlatformAccountsUpdated() {
        assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
        ThreadUtils.assertOnUiThread();
        new AsyncTask<@Nullable List<PlatformAccount>>() {
            @Override
            protected @Nullable List<PlatformAccount> doInBackground() {
                try {
                    return mDelegate.getPlatformAccountsSynchronous();
                } catch (AccountManagerDelegateException delegateException) {
                    Log.e(TAG, "Error fetching accounts from the delegate.", delegateException);
                    return null;
                }
            }

            @Override
            protected void onPostExecute(@Nullable List<PlatformAccount> allAccounts) {
                mDidAccountFetchSucceed = true;
                if (allAccounts == null) {
                    mDidAccountFetchSucceed = false;
                    if (shouldRetry()) {
                        // Wait for a fixed amount of time then try to fetch the accounts again.
                        PostTask.postDelayedTask(
                                TaskTraits.UI_USER_VISIBLE,
                                () -> {
                                    onPlatformAccountsUpdated();
                                },
                                GET_ACCOUNTS_BACKOFF_DELAY);
                        return;
                    } else {
                        // We shouldn't wait indefinitely for the account fetching to succeed, at it
                        // might block certain features. Fall back to an empty list to allow the
                        // user to proceed.
                        allAccounts =
                                mAllPlatformAccounts.get() == null
                                        ? List.of()
                                        : mAllPlatformAccounts.get();
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
                mAllPlatformAccounts.set(allAccounts);
                updateAccountInfos();
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
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            updateAccountInfos();
            return;
        }
        updateAccounts();
    }

    @MainThread
    private void updateAccounts() {
        assert !SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
        if (mAllAccounts.get() == null || mAccountRestrictionPatterns.get() == null) {
            return;
        }
        fetchGaiaIdsAndUpdateCoreAccountInfos();
    }

    @MainThread
    private void updateAccountInfos() {
        assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();

        if (mAllPlatformAccounts.get() == null || mAccountRestrictionPatterns.get() == null) {
            return;
        }

        List<AccountInfo> accounts = new ArrayList<>();
        for (PlatformAccount account : getFilteredPlatformAccounts()) {
            accounts.add(new AccountInfo.Builder(account.getEmail(), account.getId()).build());
        }

        if (mAccountsPromise.isFulfilled()) {
            mAccountsPromise = Promise.fulfilled(accounts);
        } else {
            mAccountsPromise.fulfill(accounts);
        }

        for (AccountsChangeObserver observer : mObservers) {
            observer.onCoreAccountInfosChanged();
        }
    }

    private List<PlatformAccount> getFilteredPlatformAccounts() {
        assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
        List<PlatformAccount> filteredAccounts = new ArrayList<>();
        List<PatternMatcher> restrictions = assumeNonNull(mAccountRestrictionPatterns.get());
        for (PlatformAccount account : assumeNonNull(mAllPlatformAccounts.get())) {
            String email = account.getEmail();
            boolean matches = restrictions.isEmpty();
            for (PatternMatcher matcher : restrictions) {
                if (matches) {
                    break;
                }
                matches = matcher.matches(email);
            }
            if (matches) {
                filteredAccounts.add(account);
            }
        }

        return filteredAccounts;
    }

    private List<String> getFilteredAccountEmails() {
        assert !SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
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
        mAccountsPromise = new Promise<>();
        mAllAccounts.set(null);
        if (SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled()) {
            updateAccountInfos();
            return;
        }
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
            assert !SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
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
            List<AccountInfo> accounts = new ArrayList<>();
            for (int index = 0; index < mEmails.size(); index++) {
                String email = mEmails.get(index);
                GaiaId gaiaId = gaiaIds.get(index);
                accounts.add(new AccountInfo.Builder(email, gaiaId).build());
            }
            if (mAccountsPromise.isFulfilled()) {
                mAccountsPromise = Promise.fulfilled(accounts);
            } else {
                mAccountsPromise.fulfill(accounts);
            }
            for (AccountsChangeObserver observer : mObservers) {
                observer.onCoreAccountInfosChanged();
            }
        }
    }
}
