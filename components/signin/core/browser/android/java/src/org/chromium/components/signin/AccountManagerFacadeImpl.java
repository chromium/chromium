// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.signin.util.PatternMatcher;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

/**
 * AccountManagerFacade wraps our access of AccountManager in Android.
 *
 */
public class AccountManagerFacadeImpl implements AccountManagerFacade {
    private static final String TAG = "Sync_Signin";

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

    @VisibleForTesting
    public static final String ACCOUNT_RESTRICTION_PATTERNS_KEY = "RestrictAccountsToPatterns";

    private final AccountManagerDelegate mDelegate;
    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();

    // These two variables should be accessed from either UI thread or during initialization phase.
    private PatternMatcher[] mAccountRestrictionPatterns;
    private AccountManagerResult<List<Account>> mAllAccounts;

    private final AtomicReference<AccountManagerResult<List<Account>>> mFilteredAccounts =
            new AtomicReference<>();
    private final CountDownLatch mPopulateAccountCacheLatch = new CountDownLatch(1);

    private final ArrayList<Runnable> mCallbacksWaitingForCachePopulation = new ArrayList<>();

    private int mUpdateTasksCounter;
    private final ArrayList<Runnable> mCallbacksWaitingForPendingUpdates = new ArrayList<>();
    private ObservableValue<Boolean> mUpdatePendingState = new MutableObservableValue<>(true);

    /**
     * @param delegate the AccountManagerDelegate to use as a backend
     */
    public AccountManagerFacadeImpl(AccountManagerDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
        mDelegate.registerObservers();
        mDelegate.addObserver(this::updateAccounts);

        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP) {
            subscribeToAppRestrictionChanges();
        }

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
     * Runs a callback after the account list cache is populated. In the callback
     * {@link #getGoogleAccounts()} and similar methods are guaranteed to return instantly (without
     * blocking and waiting for the cache to be populated). If the cache has already been populated,
     * the callback will be posted on UI thread.
     * @param runnable The callback to call after cache is populated. Invoked on the main thread.
     */
    @Override
    public void runAfterCacheIsPopulated(Runnable runnable) {
        ThreadUtils.assertOnUiThread();
        if (isCachePopulated()) {
            ThreadUtils.postOnUiThread(runnable);
            return;
        }
        mCallbacksWaitingForCachePopulation.add(runnable);
    }

    /**
     * Returns whether the account cache has already been populated. {@link #getGoogleAccounts()}
     * and similar methods will return instantly if the cache has been populated, otherwise these
     * methods may block waiting for the cache to be populated.
     */
    @Override
    public boolean isCachePopulated() {
        return mFilteredAccounts.get() != null;
    }

    /**
     * Retrieves all Google accounts on the device.
     *
     * @throws AccountManagerDelegateException if Google Play Services are out of date,
     *         Chrome lacks necessary permissions, etc.
     */
    @Override
    public List<Account> getGoogleAccounts() throws AccountManagerDelegateException {
        AccountManagerResult<List<Account>> maybeAccounts = mFilteredAccounts.get();
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
        return maybeAccounts.get();
    }

    /**
     * Asynchronous version of {@link #getGoogleAccounts()}.
     */
    @Override
    public void getGoogleAccounts(Callback<AccountManagerResult<List<Account>>> callback) {
        runAfterCacheIsPopulated(() -> callback.onResult(mFilteredAccounts.get()));
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
        // TODO(bsazonov): Rename delegate's getAuthToken to getAccessToken.
        return mDelegate.getAuthToken(account, scope);
    }

    /**
     * Synchronously clears an OAuth2 access token from the cache. Use {@link #getAccessToken}
     * to issue a new token after invalidating the old one.
     * @param accessToken The access token to invalidate.
     */
    @Override
    public void invalidateAccessToken(String accessToken) throws AuthException {
        assert accessToken != null;
        // TODO(bsazonov): Rename delegate's invalidateAuthToken to invalidateAccessToken.
        mDelegate.invalidateAuthToken(accessToken);
    }

    // Incorrectly infers that this is called on a worker thread because of AsyncTask doInBackground
    // overriding.
    @SuppressWarnings("WrongThread")
    @Override
    public void checkChildAccountStatus(Account account, ChildAccountStatusListener listener) {
        ThreadUtils.assertOnUiThread();
        new AsyncTask<Integer>() {
            @Override
            public @ChildAccountStatus.Status Integer doInBackground() {
                if (hasFeature(account, FEATURE_IS_CHILD_ACCOUNT_KEY)) {
                    return ChildAccountStatus.REGULAR_CHILD;
                }
                if (hasFeature(account, FEATURE_IS_USM_ACCOUNT_KEY)) {
                    return ChildAccountStatus.USM_CHILD;
                }
                return ChildAccountStatus.NOT_CHILD;
            }

            @Override
            public void onPostExecute(@ChildAccountStatus.Status Integer status) {
                listener.onStatusReady(status);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
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
     * Executes the callback after all pending account list updates finish. If there are no pending
     * account list updates, executes the callback right away.
     * @param callback the callback to be executed
     */
    @Override
    public void waitForPendingUpdates(Runnable callback) {
        ThreadUtils.assertOnUiThread();
        if (!isUpdatePending().get()) {
            callback.run();
            return;
        }
        mCallbacksWaitingForPendingUpdates.add(callback);
    }

    /**
     * Checks whether there are pending updates for account list cache.
     * @return true if there are no pending updates, false otherwise
     */
    @VisibleForTesting
    @MainThread
    public ObservableValue<Boolean> isUpdatePending() {
        ThreadUtils.assertOnUiThread();
        return mUpdatePendingState;
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

    private boolean hasFeature(Account account, String feature) {
        return mDelegate.hasFeatures(account, new String[] {feature});
    }

    private void updateAccounts() {
        ThreadUtils.assertOnUiThread();
        new UpdateAccountsTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private void updateAccountRestrictionPatterns() {
        ThreadUtils.assertOnUiThread();
        new UpdateAccountRestrictionPatternsTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private void subscribeToAppRestrictionChanges() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED);
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                updateAccountRestrictionPatterns();
            }
        };
        ContextUtils.getApplicationContext().registerReceiver(receiver, filter);
    }

    private AccountManagerResult<List<Account>> getAllAccounts() {
        try {
            List<Account> accounts = Arrays.asList(mDelegate.getAccountsSync());
            return new AccountManagerResult<>(Collections.unmodifiableList(accounts));
        } catch (AccountManagerDelegateException ex) {
            return new AccountManagerResult<>(ex);
        }
    }

    private AccountManagerResult<List<Account>> getFilteredAccounts() {
        if (mAllAccounts.hasException() || mAccountRestrictionPatterns == null) return mAllAccounts;
        ArrayList<Account> filteredAccounts = new ArrayList<>();
        for (Account account : mAllAccounts.getValue()) {
            for (PatternMatcher pattern : mAccountRestrictionPatterns) {
                if (pattern.matches(account.name)) {
                    filteredAccounts.add(account);
                    break; // Don't check other patterns
                }
            }
        }
        return new AccountManagerResult<>(Collections.unmodifiableList(filteredAccounts));
    }

    private static PatternMatcher[] getAccountRestrictionPatterns() {
        try {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return null;
            String[] patterns = getAccountRestrictionPatternPostJellyBeanMr2();
            if (patterns == null) return null;
            ArrayList<PatternMatcher> matchers = new ArrayList<>();
            for (String pattern : patterns) {
                matchers.add(new PatternMatcher(pattern));
            }
            return matchers.toArray(new PatternMatcher[0]);
        } catch (PatternMatcher.IllegalPatternException ex) {
            Log.e(TAG, "Can't get account restriction patterns", ex);
            return null;
        }
    }

    private static String[] getAccountRestrictionPatternPostJellyBeanMr2() {
        // This method uses AppRestrictions directly, rather than using the Policy interface,
        // because it must be callable in contexts in which the native library hasn't been loaded.
        Context context = ContextUtils.getApplicationContext();
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        Bundle appRestrictions = userManager.getApplicationRestrictions(context.getPackageName());
        return appRestrictions.getStringArray(ACCOUNT_RESTRICTION_PATTERNS_KEY);
    }

    private void setAccountRestrictionPatterns(PatternMatcher[] patternMatchers) {
        mAccountRestrictionPatterns = patternMatchers;
        mFilteredAccounts.set(getFilteredAccounts());
        fireOnAccountsChangedNotification();
    }

    private void setAllAccounts(AccountManagerResult<List<Account>> allAccounts) {
        mAllAccounts = allAccounts;
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

        mUpdatePendingState.set(true);
    }

    private void decrementUpdateCounter() {
        assert mUpdateTasksCounter > 0;
        if (--mUpdateTasksCounter > 0) return;

        for (Runnable callback : mCallbacksWaitingForPendingUpdates) {
            callback.run();
        }
        mCallbacksWaitingForPendingUpdates.clear();
        mUpdatePendingState.set(false);
    }

    private class InitializeTask extends AsyncTask<Void> {
        @Override
        protected void onPreExecute() {
            incrementUpdateCounter();
        }

        @Override
        protected Void doInBackground() {
            mAccountRestrictionPatterns = getAccountRestrictionPatterns();
            mAllAccounts = getAllAccounts();
            mFilteredAccounts.set(getFilteredAccounts());
            // It's important that countDown() is called on background thread and not in
            // onPostExecute, as UI thread may be blocked in getGoogleAccounts waiting on the latch.
            mPopulateAccountCacheLatch.countDown();
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            // Records number of Android accounts present on device.
            RecordHistogram.recordExactLinearHistogram(
                    "Signin.AndroidNumberOfDeviceAccounts", tryGetGoogleAccounts().size(), 50);

            for (Runnable callback : mCallbacksWaitingForCachePopulation) {
                callback.run();
            }
            mCallbacksWaitingForCachePopulation.clear();

            fireOnAccountsChangedNotification();
            decrementUpdateCounter();
        }
    }

    private class UpdateAccountRestrictionPatternsTask extends AsyncTask<PatternMatcher[]> {
        @Override
        protected void onPreExecute() {
            incrementUpdateCounter();
        }

        @Override
        protected PatternMatcher[] doInBackground() {
            return getAccountRestrictionPatterns();
        }

        @Override
        protected void onPostExecute(PatternMatcher[] patternMatchers) {
            setAccountRestrictionPatterns(patternMatchers);
            decrementUpdateCounter();
        }
    }

    private class UpdateAccountsTask extends AsyncTask<AccountManagerResult<List<Account>>> {
        @Override
        protected void onPreExecute() {
            incrementUpdateCounter();
        }

        @Override
        protected AccountManagerResult<List<Account>> doInBackground() {
            return getAllAccounts();
        }

        @Override
        protected void onPostExecute(AccountManagerResult<List<Account>> allAccounts) {
            setAllAccounts(allAccounts);
            decrementUpdateCounter();
        }
    }
}
