// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.signin.util.PatternMatcher;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;
import java.util.regex.Pattern;

/**
 * AccountManagerFacade wraps our access of AccountManager in Android.
 *
 * Use the {@link #initializeAccountManagerFacade} to instantiate it.
 * After initialization, instance get be acquired by calling {@link #get}.
 */
public class AccountManagerFacade {
    private static final String TAG = "Sync_Signin";
    private static final Pattern AT_SYMBOL = Pattern.compile("@");
    private static final String GMAIL_COM = "gmail.com";
    private static final String GOOGLEMAIL_COM = "googlemail.com";
    public static final String GOOGLE_ACCOUNT_TYPE = "com.google";

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

    private static AccountManagerFacade sInstance;
    private static AccountManagerFacade sTestingInstance;

    private static final AtomicReference<AccountManagerFacade> sAtomicInstance =
            new AtomicReference<>();

    private final AccountManagerDelegate mDelegate;
    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();

    // These two variables should be accessed from either UI thread or during initialization phase.
    private PatternMatcher[] mAccountRestrictionPatterns;
    private AccountManagerResult<List<Account>> mAllAccounts;

    private final AtomicReference<AccountManagerResult<List<Account>>> mFilteredAccounts =
            new AtomicReference<>();
    private final CountDownLatch mPopulateAccountCacheLatch = new CountDownLatch(1);
    private final CachedMetrics.TimesHistogramSample mPopulateAccountCacheWaitingTimeHistogram =
            new CachedMetrics.TimesHistogramSample("Signin.AndroidPopulateAccountCacheWaitingTime");

    private final ArrayList<Runnable> mCallbacksWaitingForCachePopulation = new ArrayList<>();

    private int mUpdateTasksCounter;
    private final ArrayList<Runnable> mCallbacksWaitingForPendingUpdates = new ArrayList<>();
    private ObservableValue<Boolean> mUpdatePendingState = new MutableObservableValue<>(true);

    /**
     * @param delegate the AccountManagerDelegate to use as a backend
     */
    private AccountManagerFacade(AccountManagerDelegate delegate) {
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
     * Initializes AccountManagerFacade singleton instance. Can only be called once.
     * Tests can override the instance with {@link #overrideAccountManagerFacadeForTests}.
     *
     * @param delegate the AccountManagerDelegate to use
     */
    @MainThread
    public static void initializeAccountManagerFacade(AccountManagerDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        if (sInstance != null) {
            throw new IllegalStateException("AccountManagerFacade is already initialized!");
        }
        sInstance = new AccountManagerFacade(delegate);
        if (sTestingInstance != null) return;
        sAtomicInstance.set(sInstance);
    }

    /**
     * Overrides AccountManagerFacade singleton instance for tests. Only for use in Tests.
     * Overrides any previous or future calls to {@link #initializeAccountManagerFacade}.
     *
     * @param delegate the AccountManagerDelegate to use
     */
    @VisibleForTesting
    @AnyThread
    public static void overrideAccountManagerFacadeForTests(AccountManagerDelegate delegate) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            sTestingInstance = new AccountManagerFacade(delegate);
            sAtomicInstance.set(sTestingInstance);
        });
    }

    /**
     * Resets custom AccountManagerFacade set with {@link #overrideAccountManagerFacadeForTests}.
     * Only for use in Tests.
     */
    @VisibleForTesting
    @AnyThread
    public static void resetAccountManagerFacadeForTests() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            sTestingInstance = null;
            sAtomicInstance.set(sInstance);
        });
    }

    /**
     * Singleton instance getter. Singleton must be initialized before calling this by
     * {@link #initializeAccountManagerFacade} or {@link #overrideAccountManagerFacadeForTests}.
     *
     * @return a singleton instance
     */
    @AnyThread
    @CalledByNative
    public static AccountManagerFacade get() {
        AccountManagerFacade instance = sAtomicInstance.get();
        assert instance != null : "AccountManagerFacade is not initialized!";
        return instance;
    }

    /**
     * Adds an observer to receive accounts change notifications.
     * @param observer the observer to add.
     */
    @MainThread
    public void addObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        boolean success = mObservers.addObserver(observer);
        assert success : "Observer already added!";
    }

    /**
     * Removes an observer that was previously added using {@link #addObserver}.
     * @param observer the observer to remove.
     */
    @MainThread
    public void removeObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        boolean success = mObservers.removeObserver(observer);
        assert success : "Can't find observer";
    }

    /**
     * Creates an Account object for the given name.
     */
    @AnyThread
    public static Account createAccountFromName(String name) {
        return new Account(name, GOOGLE_ACCOUNT_TYPE);
    }

    /**
     * Runs a callback after the account list cache is populated. In the callback
     * {@link #getGoogleAccounts()} and similar methods are guaranteed to return instantly (without
     * blocking and waiting for the cache to be populated). If the cache has already been populated,
     * the callback will be posted on UI thread.
     * @param runnable The callback to call after cache is populated. Invoked on the main thread.
     */
    @MainThread
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
    @AnyThread
    public boolean isCachePopulated() {
        return mFilteredAccounts.get() != null;
    }

    /**
     * Retrieves a list of the Google account names on the device.
     *
     * @throws AccountManagerDelegateException if Google Play Services are out of date,
     *         Chrome lacks necessary permissions, etc.
     */
    @AnyThread
    public List<String> getGoogleAccountNames() throws AccountManagerDelegateException {
        List<String> accountNames = new ArrayList<>();
        for (Account account : getGoogleAccounts()) {
            accountNames.add(account.name);
        }
        return accountNames;
    }

    /**
     * Retrieves a list of the Google account names on the device.
     * Returns an empty list if Google Play Services aren't available or out of date.
     */
    @AnyThread
    public List<String> tryGetGoogleAccountNames() {
        List<String> accountNames = new ArrayList<>();
        List<Account> tryGetGoogleAccounts = tryGetGoogleAccounts();
        for (int i = 0; i < tryGetGoogleAccounts.size(); i++) {
            Account account = tryGetGoogleAccounts.get(i);
            accountNames.add(account.name);
        }
        return accountNames;
    }

    /**
     * Asynchronous version of {@link #tryGetGoogleAccountNames()}.
     */
    @MainThread
    public void tryGetGoogleAccountNames(final Callback<List<String>> callback) {
        runAfterCacheIsPopulated(() -> callback.onResult(tryGetGoogleAccountNames()));
    }

    /**
     * Asynchronous version of {@link #tryGetGoogleAccountNames()}.
     */
    @MainThread
    public void getGoogleAccountNames(
            final Callback<AccountManagerResult<List<String>>> callback) {
        runAfterCacheIsPopulated(() -> {
            final AccountManagerResult<List<Account>> accounts = mFilteredAccounts.get();
            final AccountManagerResult<List<String>> result;
            if (accounts.hasValue()) {
                List<String> accountNames = new ArrayList<>(accounts.getValue().size());
                for (Account account : accounts.getValue()) {
                    accountNames.add(account.name);
                }
                result = new AccountManagerResult<>(accountNames);
            } else {
                result = new AccountManagerResult<>(accounts.getException());
            }
            callback.onResult(result);
        });
    }

    /**
     * Retrieves all Google accounts on the device.
     *
     * @throws AccountManagerDelegateException if Google Play Services are out of date,
     *         Chrome lacks necessary permissions, etc.
     */
    @AnyThread
    public List<Account> getGoogleAccounts() throws AccountManagerDelegateException {
        AccountManagerResult<List<Account>> maybeAccounts = mFilteredAccounts.get();
        if (maybeAccounts == null) {
            try {
                // First call to update hasn't finished executing yet, should wait for it
                long now = SystemClock.elapsedRealtime();
                mPopulateAccountCacheLatch.await();
                maybeAccounts = mFilteredAccounts.get();
                if (ThreadUtils.runningOnUiThread()) {
                    mPopulateAccountCacheWaitingTimeHistogram.record(
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
    @MainThread
    public void getGoogleAccounts(Callback<AccountManagerResult<List<Account>>> callback) {
        runAfterCacheIsPopulated(() -> callback.onResult(mFilteredAccounts.get()));
    }

    /**
     * Retrieves all Google accounts on the device.
     * Returns an empty array if an error occurs while getting account list.
     */
    @AnyThread
    public List<Account> tryGetGoogleAccounts() {
        try {
            return getGoogleAccounts();
        } catch (AccountManagerDelegateException e) {
            return Collections.emptyList();
        }
    }

    /**
     * Asynchronous version of {@link #tryGetGoogleAccounts()}.
     */
    @MainThread
    public void tryGetGoogleAccounts(final Callback<List<Account>> callback) {
        runAfterCacheIsPopulated(() -> callback.onResult(tryGetGoogleAccounts()));
    }

    /**
     * Determine whether there are any Google accounts on the device.
     * Returns false if an error occurs while getting account list.
     */
    @AnyThread
    public boolean hasGoogleAccounts() {
        return !tryGetGoogleAccounts().isEmpty();
    }

    /**
     * Asynchronous version of {@link #hasGoogleAccounts()}.
     */
    @MainThread
    public void hasGoogleAccounts(final Callback<Boolean> callback) {
        runAfterCacheIsPopulated(() -> callback.onResult(hasGoogleAccounts()));
    }

    private String canonicalizeName(String name) {
        String[] parts = AT_SYMBOL.split(name);
        if (parts.length != 2) return name;

        if (GOOGLEMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[1] = GMAIL_COM;
        }
        if (GMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[0] = parts[0].replace(".", "");
        }
        return (parts[0] + "@" + parts[1]).toLowerCase(Locale.US);
    }

    /**
     * Returns the account if it exists; null if account doesn't exists or an error occurs
     * while getting account list.
     */
    @AnyThread
    public Account getAccountFromName(String accountName) {
        String canonicalName = canonicalizeName(accountName);
        List<Account> accounts = tryGetGoogleAccounts();
        for (Account account : accounts) {
            if (canonicalizeName(account.name).equals(canonicalName)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Asynchronous version of {@link #getAccountFromName(String)}.
     */
    @MainThread
    public void getAccountFromName(String accountName, final Callback<Account> callback) {
        runAfterCacheIsPopulated(() -> callback.onResult(getAccountFromName(accountName)));
    }

    /**
     * Returns whether an account exists with the given name.
     * Returns false if an error occurs while getting account list.
     */
    @AnyThread
    public boolean hasAccountForName(String accountName) {
        return getAccountFromName(accountName) != null;
    }

    /**
     * Asynchronous version of {@link #hasAccountForName(String)}.
     */
    // TODO(maxbogue): Remove once this function is used outside of tests.
    @VisibleForTesting
    @MainThread
    public void hasAccountForName(String accountName, final Callback<Boolean> callback) {
        runAfterCacheIsPopulated(() -> callback.onResult(hasAccountForName(accountName)));
    }

    /**
     * @return Whether or not there is an account authenticator for Google accounts.
     */
    @AnyThread
    public boolean hasGoogleAccountAuthenticator() {
        AuthenticatorDescription[] descs = mDelegate.getAuthenticatorTypes();
        for (AuthenticatorDescription desc : descs) {
            if (GOOGLE_ACCOUNT_TYPE.equals(desc.type)) return true;
        }
        return false;
    }

    /**
     * Synchronously gets an OAuth2 access token. May return a cached version, use
     * {@link #invalidateAccessToken} to invalidate a token in the cache.
     * @param account The {@link Account} for which the token is requested.
     * @param scope OAuth2 scope for which the requested token should be valid.
     * @return The OAuth2 access token as a string.
     */
    @WorkerThread
    String getAccessToken(Account account, String scope) throws AuthException {
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
    @WorkerThread
    void invalidateAccessToken(String accessToken) throws AuthException {
        assert accessToken != null;
        // TODO(bsazonov): Rename delegate's invalidateAuthToken to invalidateAccessToken.
        mDelegate.invalidateAuthToken(accessToken);
    }

    // Incorrectly infers that this is called on a worker thread because of AsyncTask doInBackground
    // overriding.
    @SuppressWarnings("WrongThread")
    @MainThread
    public void checkChildAccountStatus(Account account, Callback<Integer> callback) {
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
            public void onPostExecute(@ChildAccountStatus.Status Integer value) {
                callback.onResult(value);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Creates an intent that will ask the user to add a new account to the device. See
     * {@link AccountManager#addAccount} for details.
     * @param callback The callback to get the created intent. Will be invoked on the main thread.
     *         If there is an issue while creating the intent, callback will receive null.
     */
    @AnyThread
    public void createAddAccountIntent(Callback<Intent> callback) {
        mDelegate.createAddAccountIntent(callback);
    }

    /**
     * Asks the user to enter a new password for an account, updating the saved credentials for the
     * account.
     */
    @MainThread
    public void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback) {
        mDelegate.updateCredentials(account, activity, callback);
    }

    /**
     * Gets profile data source.
     * @return {@link ProfileDataSource} if it is supported by implementation, null otherwise.
     */
    @MainThread
    @Nullable
    public ProfileDataSource getProfileDataSource() {
        return mDelegate.getProfileDataSource();
    }

    /**
     * Executes the callback after all pending account list updates finish. If there are no pending
     * account list updates, executes the callback right away.
     * @param callback the callback to be executed
     */
    @MainThread
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
    @MainThread
    public ObservableValue<Boolean> isUpdatePending() {
        ThreadUtils.assertOnUiThread();
        return mUpdatePendingState;
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

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
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

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static String[] getAccountRestrictionPatternPostJellyBeanMr2() {
        // This method uses AppRestrictions directly, rather than using the Policy interface,
        // because it must be callable in contexts in which the native library hasn't been loaded.
        Context context = ContextUtils.getApplicationContext();
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        Bundle appRestrictions = userManager.getApplicationRestrictions(context.getPackageName());
        // TODO(https://crbug.com/779568): Remove this after migrating to Robolectric 3.7+.
        // Android guarantees that getApplicationRestrictions result won't be null, but
        // Robolectric versions 3.6 and older don't respect this.
        if (appRestrictions == null) appRestrictions = new Bundle();
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
