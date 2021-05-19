// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.Intent;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.common.base.Optional;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

/**
 * AccountManagerFacade wraps our access of AccountManager in Android.
 */
public class AccountManagerFacadeImpl implements AccountManagerFacade {
    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a child account.
     */
    @VisibleForTesting
    public static final String FEATURE_IS_CHILD_ACCOUNT_KEY = "service_uca";

    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a USM account.
     */
    @VisibleForTesting
    public static final String FEATURE_IS_USM_ACCOUNT_KEY = "service_usm";

    private final AccountManagerDelegate mDelegate;
    private final AccountRestrictionPatternReceiver mAccountRestrictionPatternReceiver;

    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();

    private final AtomicReference<List<Account>> mAllAccounts = new AtomicReference<>();
    private final AtomicReference<List<PatternMatcher>> mAccountRestrictionPatterns =
            new AtomicReference<>();
    private final AtomicReference<List<Account>> mFilteredAccounts = new AtomicReference<>();
    private final CountDownLatch mPopulateAccountCacheLatch = new CountDownLatch(1);

    private int mUpdateTasksCounter;
    private final Queue<Callback<List<Account>>> mCallbacksWaitingForAccountsFetch =
            new ArrayDeque<>();
    // The map stores the boolean for whether an account is subject to minor mode restrictions
    private final Map<String, Boolean> mSubjectToMinorModeRestrictions = new HashMap<>();

    /**
     * @param delegate the AccountManagerDelegate to use as a backend
     */
    public AccountManagerFacadeImpl(AccountManagerDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
        mDelegate.attachAccountsChangeObserver(this::updateAccounts);
        mAccountRestrictionPatternReceiver =
                new AccountRestrictionPatternReceiver(this::onAccountRestrictionPatternsUpdated);

        tryGetGoogleAccounts(accounts -> {
            RecordHistogram.recordExactLinearHistogram(
                    "Signin.AndroidNumberOfDeviceAccounts", accounts.size(), 50);
        });
        new InitializeTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
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

    /**
     * Returns whether the account cache has already been populated. {@link #tryGetGoogleAccounts()}
     * and similar methods will return instantly if the cache has been populated, otherwise these
     * methods may block waiting for the cache to be populated.
     */
    @Override
    public boolean isCachePopulated() {
        return mFilteredAccounts.get() != null;
    }

    @Override
    public Optional<List<Account>> getGoogleAccounts() {
        return Optional.fromNullable(mFilteredAccounts.get());
    }

    @Override
    public List<Account> tryGetGoogleAccounts() {
        List<Account> maybeAccounts = mFilteredAccounts.get();
        if (maybeAccounts == null) {
            try {
                // First call to update hasn't finished executing yet, should wait for it
                long now = SystemClock.elapsedRealtime();
                mPopulateAccountCacheLatch.await();
                maybeAccounts = mFilteredAccounts.get();
                if (ThreadUtils.runningOnUiThread()) {
                    RecordHistogram.recordTimesHistogram(
                            "Signin.AndroidPopulateAccountCacheWaitingTime",
                            SystemClock.elapsedRealtime() - now);
                }
            } catch (InterruptedException e) {
                throw new RuntimeException("Interrupted waiting for accounts", e);
            }
        }
        return maybeAccounts;
    }

    /**
     * Asynchronous version of {@link #tryGetGoogleAccounts()}.
     */
    @Override
    public void tryGetGoogleAccounts(Callback<List<Account>> callback) {
        ThreadUtils.assertOnUiThread();
        if (isCachePopulated()) {
            ThreadUtils.postOnUiThread(callback.bind(tryGetGoogleAccounts()));
        } else {
            mCallbacksWaitingForAccountsFetch.add(callback);
        }
    }

    /**
     * @return Whether or not there is an account authenticator for Google accounts.
     */
    @Override
    public boolean hasGoogleAccountAuthenticator() {
        AuthenticatorDescription[] descs = mDelegate.getAuthenticatorTypes();
        for (AuthenticatorDescription desc : descs) {
            if (AccountUtils.GOOGLE_ACCOUNT_TYPE.equals(desc.type)) return true;
        }
        return false;
    }

    /**
     * Synchronously gets an OAuth2 access token. May return a cached version, use
     * {@link #invalidateAccessToken} to invalidate a token in the cache.
     * @param account The {@link Account} for which the token is requested.
     * @param scope OAuth2 scope for which the requested token should be valid.
     * @return The OAuth2 access token as an AccessTokenData with a string and an expiration time..
     */
    @Override
    public AccessTokenData getAccessToken(Account account, String scope) throws AuthException {
        assert account != null;
        assert scope != null;
        return mDelegate.getAuthToken(account, scope);
    }

    /**
     * Removes an OAuth2 access token from the cache with retries asynchronously.
     * Uses {@link #getAccessToken} to issue a new token after invalidating the old one.
     * @param accessToken The access token to invalidate.
     */
    @Override
    public void invalidateAccessToken(String accessToken) {
        if (!TextUtils.isEmpty(accessToken)) {
            ConnectionRetry.runAuthTask(() -> {
                mDelegate.invalidateAuthToken(accessToken);
                return true;
            });
        }
    }

    @Override
    public void checkChildAccountStatus(Account account, ChildAccountStatusListener listener) {
        ThreadUtils.assertOnUiThread();
        new AsyncTask<Integer>() {
            @Override
            public @ChildAccountStatus.Status Integer doInBackground() {
                if (mDelegate.hasFeature(account, FEATURE_IS_CHILD_ACCOUNT_KEY)) {
                    return ChildAccountStatus.REGULAR_CHILD;
                } else if (mDelegate.hasFeature(account, FEATURE_IS_USM_ACCOUNT_KEY)) {
                    return ChildAccountStatus.USM_CHILD;
                } else {
                    return ChildAccountStatus.NOT_CHILD;
                }
            }

            @Override
            public void onPostExecute(@ChildAccountStatus.Status Integer status) {
                listener.onStatusReady(status);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public Optional<Boolean> isAccountSubjectToMinorModeRestrictions(Account account) {
        return Optional.fromNullable(
                mSubjectToMinorModeRestrictions.get(AccountUtils.canonicalizeName(account.name)));
    }

    /**
     * Creates an intent that will ask the user to add a new account to the device. See
     * {@link AccountManager#addAccount} for details.
     * @param callback The callback to get the created intent. Will be invoked on the main thread.
     *         If there is an issue while creating the intent, callback will receive null.
     */
    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {
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

    /**
     * Gets profile data source.
     * @return {@link ProfileDataSource} if it is supported by implementation, null otherwise.
     */
    @Override
    public ProfileDataSource getProfileDataSource() {
        return mDelegate.getProfileDataSource();
    }

    /**
     * Returns the Gaia id for the account associated with the given email address.
     * If an account with the given email address is not installed on the device
     * then null is returned.
     *
     * This method will throw IllegalStateException if called on the main thread.
     *
     * @param accountEmail The email address of a Google account.
     */
    @Override
    public String getAccountGaiaId(String accountEmail) {
        return mDelegate.getAccountGaiaId(accountEmail);
    }

    /**
     * Checks whether Google Play services is available.
     */
    @Override
    public boolean isGooglePlayServicesAvailable() {
        return mDelegate.isGooglePlayServicesAvailable();
    }

    private void updateAccounts() {
        ThreadUtils.assertOnUiThread();
        new UpdateAccountsTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private List<Account> getAllAccounts() {
        return Collections.unmodifiableList(Arrays.asList(mDelegate.getAccounts()));
    }

    private List<Account> getFilteredAccounts() {
        if (mAccountRestrictionPatterns.get().isEmpty()) {
            return mAllAccounts.get();
        }
        final List<Account> filteredAccounts = new ArrayList<>();
        for (Account account : mAllAccounts.get()) {
            for (PatternMatcher pattern : mAccountRestrictionPatterns.get()) {
                if (pattern.matches(account.name)) {
                    filteredAccounts.add(account);
                    break; // Don't check other patterns
                }
            }
        }
        return Collections.unmodifiableList(filteredAccounts);
    }

    private void onAccountRestrictionPatternsUpdated(List<PatternMatcher> patternMatchers) {
        mAccountRestrictionPatterns.set(patternMatchers);
        mFilteredAccounts.set(getFilteredAccounts());
        fireOnAccountsChangedNotification();
    }

    private void setAllAccounts(List<Account> allAccounts) {
        mAllAccounts.set(allAccounts);
        mFilteredAccounts.set(getFilteredAccounts());
        fireOnAccountsChangedNotification();
    }

    private void fireOnAccountsChangedNotification() {
        for (AccountsChangeObserver observer : mObservers) {
            observer.onAccountsChanged();
        }
    }

    private void incrementUpdateCounter() {
        assert mUpdateTasksCounter >= 0;
        if (mUpdateTasksCounter++ > 0) return;
    }

    private void decrementUpdateCounter() {
        assert mUpdateTasksCounter > 0;
        if (--mUpdateTasksCounter > 0) return;

        while (!mCallbacksWaitingForAccountsFetch.isEmpty()) {
            final Callback<List<Account>> callback = mCallbacksWaitingForAccountsFetch.remove();
            callback.onResult(tryGetGoogleAccounts());
        }
    }

    private class InitializeTask extends AsyncTask<Void> {
        @Override
        protected void onPreExecute() {
            incrementUpdateCounter();
        }

        @Override
        protected Void doInBackground() {
            mAccountRestrictionPatterns.set(
                    mAccountRestrictionPatternReceiver.getRestrictionPatterns());
            mAllAccounts.set(getAllAccounts());
            mFilteredAccounts.set(getFilteredAccounts());
            // It's important that countDown() is called on background thread and not in
            // onPostExecute, as UI thread may be blocked in getGoogleAccounts waiting on the latch.
            mPopulateAccountCacheLatch.countDown();
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            while (!mCallbacksWaitingForAccountsFetch.isEmpty()) {
                final Callback<List<Account>> callback = mCallbacksWaitingForAccountsFetch.remove();
                callback.onResult(mFilteredAccounts.get());
            }
            fireOnAccountsChangedNotification();
            decrementUpdateCounter();
        }
    }

    private class UpdateAccountsTask extends AsyncTask<List<Account>> {
        @Override
        protected void onPreExecute() {
            incrementUpdateCounter();
        }

        @Override
        protected List<Account> doInBackground() {
            return getAllAccounts();
        }

        @Override
        protected void onPostExecute(List<Account> allAccounts) {
            setAllAccounts(allAccounts);
            decrementUpdateCounter();
        }
    }
}
