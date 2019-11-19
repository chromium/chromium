// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.AsyncTask;
import org.chromium.net.NetworkChangeNotifier;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Java instance for the native OAuth2TokenService.
 * <p/>
 * This class forwards calls to request or invalidate access tokens made by native code to
 * AccountManagerFacade and forwards callbacks to native code.
 * <p/>
 */
public final class OAuth2TokenService
        implements AccountTrackerService.OnSystemAccountsSeededListener {
    private static final String TAG = "OAuth2TokenService";

    @VisibleForTesting
    public static final String STORED_ACCOUNTS_KEY = "google.services.stored_accounts";

    /**
     * A simple callback for getAccessToken.
     */
    public interface GetAccessTokenCallback {
        /**
         * Invoked on the UI thread if a token is provided by the AccountManager.
         *
         * @param token Access token, guaranteed not to be null.
         */
        void onGetTokenSuccess(String token);

        /**
         * Invoked on the UI thread if no token is available.
         *
         * @param isTransientError Indicates if the error is transient (network timeout or
         * unavailable, etc) or persistent (bad credentials, permission denied, etc).
         */
        void onGetTokenFailure(boolean isTransientError);
    }

    private static final String OAUTH2_SCOPE_PREFIX = "oauth2:";

    private final long mNativeOAuth2TokenServiceDelegate;
    private final AccountTrackerService mAccountTrackerService;
    private final AccountManagerFacade mAccountManagerFacade;

    private boolean mPendingUpdate;
    // TODO(crbug.com/934688) Once OAuth2TokenService.java is internalized, use CoreAccountId
    // instead of String.
    private String mPendingUpdateAccountId;

    @VisibleForTesting
    public OAuth2TokenService(long nativeOAuth2TokenServiceDelegate,
            AccountTrackerService accountTrackerService,
            AccountManagerFacade accountManagerFacade) {
        mNativeOAuth2TokenServiceDelegate = nativeOAuth2TokenServiceDelegate;
        mAccountTrackerService = accountTrackerService;
        mAccountManagerFacade = accountManagerFacade;

        // AccountTrackerService might be null in tests.
        if (mAccountTrackerService != null) {
            mAccountTrackerService.addSystemAccountsSeededListener(this);
        }
    }

    @CalledByNative
    private static OAuth2TokenService create(long nativeOAuth2TokenServiceDelegate,
            AccountTrackerService accountTrackerService,
            AccountManagerFacade accountManagerFacade) {
        assert nativeOAuth2TokenServiceDelegate != 0;
        return new OAuth2TokenService(
                nativeOAuth2TokenServiceDelegate, accountTrackerService, accountManagerFacade);
    }

    private Account getAccountOrNullFromUsername(String username) {
        if (username == null) {
            Log.e(TAG, "Username is null");
            return null;
        }

        Account account = mAccountManagerFacade.getAccountFromName(username);
        if (account == null) {
            Log.e(TAG, "Account not found for provided username.");
            return null;
        }
        return account;
    }

    /**
     * Called by native to list the active account names in the OS.
     */
    @VisibleForTesting
    @CalledByNative
    public String[] getSystemAccountNames() {
        // TODO(https://crbug.com/768366): Remove this after adding cache to account manager facade.
        // This function is called by native code on UI thread.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            List<String> accountNames = mAccountManagerFacade.tryGetGoogleAccountNames();
            return accountNames.toArray(new String[accountNames.size()]);
        }
    }

    /**
     * Called by native to list the accounts Id with OAuth2 refresh tokens.
     * This can differ from getSystemAccountNames as the user add/remove accounts
     * from the OS. updateAccountList should be called to keep these two
     * in sync.
     */
    @VisibleForTesting
    @CalledByNative
    public static String[] getAccounts() {
        return getStoredAccounts();
    }

    /**
     * Called by native to retrieve OAuth2 tokens.
     * @param username The native username (email address).
     * @param scope The scope to get an auth token for (without Android-style 'oauth2:' prefix).
     * @param nativeCallback The pointer to the native callback that should be run upon completion.
     */
    @MainThread
    @CalledByNative
    private void getAccessTokenFromNative(
            String username, String scope, final long nativeCallback) {
        Account account = getAccountOrNullFromUsername(username);
        if (account == null) {
            ThreadUtils.postOnUiThread(() -> {
                OAuth2TokenServiceJni.get().onOAuth2TokenFetched(null, false, nativeCallback);
            });
            return;
        }
        String oauth2Scope = OAUTH2_SCOPE_PREFIX + scope;
        getAccessToken(account, oauth2Scope, new GetAccessTokenCallback() {
            @Override
            public void onGetTokenSuccess(String token) {
                OAuth2TokenServiceJni.get().onOAuth2TokenFetched(token, false, nativeCallback);
            }

            @Override
            public void onGetTokenFailure(boolean isTransientError) {
                OAuth2TokenServiceJni.get().onOAuth2TokenFetched(
                        null, isTransientError, nativeCallback);
            }
        });
    }

    /**
     * Call this method to retrieve an OAuth2 access token for the given account and scope. Please
     * note that this method expects a scope with 'oauth2:' prefix.
     * @param account the account to get the access token for.
     * @param scope The scope to get an auth token for (with Android-style 'oauth2:' prefix).
     * @param callback called on successful and unsuccessful fetching of auth token.
     */
    @MainThread
    public void getAccessToken(Account account, String scope, GetAccessTokenCallback callback) {
        getAccessTokenWithFacade(mAccountManagerFacade, account, scope, callback);
    }

    /**
     * Call this method to retrieve an OAuth2 access token for the given account and scope. Please
     * note that this method expects a scope with 'oauth2:' prefix.
     *
     * @deprecated Use getAccessToken instead. crbug.com/1014098: This method is available as a
     *         workaround for a callsite where native is not initialized yet.
     *
     * @param accountManagerFacade AccountManagerFacade to request the access token from.
     * @param account the account to get the access token for.
     * @param scope The scope to get an auth token for (with Android-style 'oauth2:' prefix).
     * @param callback called on successful and unsuccessful fetching of auth token.
     */
    @MainThread
    @Deprecated
    public static void getAccessTokenWithFacade(AccountManagerFacade accountManagerFacade,
            Account account, String scope, GetAccessTokenCallback callback) {
        ConnectionRetry.runAuthTask(new AuthTask<String>() {
            @Override
            public String run() throws AuthException {
                return accountManagerFacade.getAccessToken(account, scope);
            }
            @Override
            public void onSuccess(String token) {
                callback.onGetTokenSuccess(token);
            }
            @Override
            public void onFailure(boolean isTransientError) {
                callback.onGetTokenFailure(isTransientError);
            }
        });
    }

    /**
     * Called by native to invalidate an OAuth2 token. Please note that the token is invalidated
     * asynchronously.
     */
    @MainThread
    @CalledByNative
    public void invalidateAccessToken(String accessToken) {
        if (TextUtils.isEmpty(accessToken)) {
            return;
        }
        ConnectionRetry.runAuthTask(new AuthTask<Boolean>() {
            @Override
            public Boolean run() throws AuthException {
                mAccountManagerFacade.invalidateAccessToken(accessToken);
                return true;
            }
            @Override
            public void onSuccess(Boolean result) {}
            @Override
            public void onFailure(boolean isTransientError) {
                Log.e(TAG, "Failed to invalidate auth token: " + accessToken);
            }
        });
    }

    /**
     * Invalidates the old token (if non-null/non-empty) and asynchronously generates a new one.
     *
     * @deprecated Use invalidateAccessToken and getAccessToken instead. crbug.com/1002894: This
     *         method is needed by InvalidationClientService which is not necessary anymore.
     *
     * @param account the account to get the access token for.
     * @param oldToken The old token to be invalidated or null.
     * @param scope The scope to get an auth token for (with Android-style 'oauth2:' prefix).
     * @param callback called on successful and unsuccessful fetching of auth token.
     */
    @Deprecated
    public static void getNewAccessTokenWithFacade(AccountManagerFacade accountManagerFacade,
            Account account, @Nullable String oldToken, String scope,
            GetAccessTokenCallback callback) {
        ConnectionRetry.runAuthTask(new AuthTask<String>() {
            @Override
            public String run() throws AuthException {
                if (!TextUtils.isEmpty(oldToken)) {
                    accountManagerFacade.invalidateAccessToken(oldToken);
                }
                return accountManagerFacade.getAccessToken(account, scope);
            }
            @Override
            public void onSuccess(String token) {
                callback.onGetTokenSuccess(token);
            }
            @Override
            public void onFailure(boolean isTransientError) {
                callback.onGetTokenFailure(isTransientError);
            }
        });
    }

    /**
     * Called by native to check whether the account has an OAuth2 refresh token.
     */
    @CalledByNative
    private boolean hasOAuth2RefreshToken(String accountName) {
        if (!mAccountManagerFacade.isCachePopulated()) {
            return false;
        }

        // Temporarily allowing disk read while fixing. TODO: http://crbug.com/618096.
        // This function is called in RefreshTokenIsAvailable of OAuth2TokenService which is
        // expected to be called in the UI thread synchronously.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return AccountManagerFacade.get().hasAccountForName(accountName);
        }
    }

    /**
     * Continue pending accounts validation after system accounts have been seeded into
     * AccountTrackerService.
     */
    @Override
    public void onSystemAccountsSeedingComplete() {
        if (mPendingUpdate) {
            reloadAllAccountsWithPrimaryAccountAfterSeeding(mPendingUpdateAccountId);
            mPendingUpdate = false;
            mPendingUpdateAccountId = null;
        }
    }

    @CalledByNative
    private void seedAndReloadAccountsWithPrimaryAccount(@Nullable String accountId) {
        ThreadUtils.assertOnUiThread();
        if (!mAccountTrackerService.checkAndSeedSystemAccounts()) {
            assert !mPendingUpdate && mPendingUpdateAccountId == null;
            mPendingUpdate = true;
            mPendingUpdateAccountId = accountId;
            return;
        }

        reloadAllAccountsWithPrimaryAccountAfterSeeding(accountId);
    }

    private void reloadAllAccountsWithPrimaryAccountAfterSeeding(@Nullable String accountId) {
        OAuth2TokenServiceJni.get().reloadAllAccountsWithPrimaryAccountAfterSeeding(
                mNativeOAuth2TokenServiceDelegate, accountId);
    }

    private static String[] getStoredAccounts() {
        Set<String> accounts =
                ContextUtils.getAppSharedPreferences().getStringSet(STORED_ACCOUNTS_KEY, null);
        return accounts == null ? new String[] {} : accounts.toArray(new String[0]);
    }

    /**
     * Called by native to save the account IDs that have associated OAuth2 refresh tokens.
     * This is called during updateAccountList to sync with getSystemAccountNames.
     * @param accounts IDs to save.
     */
    @CalledByNative
    private static void setAccounts(String[] accounts) {
        Set<String> set = new HashSet<>(Arrays.asList(accounts));
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putStringSet(STORED_ACCOUNTS_KEY, set)
                .apply();
    }

    private interface AuthTask<T> {
        T run() throws AuthException;
        void onSuccess(T result);
        void onFailure(boolean isTransientError);
    }

    /**
     * A helper class to encapsulate network connection retry logic for AuthTasks.
     *
     * The task will be run on the background thread. If it encounters a transient error, it will
     * wait for a network change and retry up to MAX_TRIES times.
     */
    private static class ConnectionRetry<T>
            implements NetworkChangeNotifier.ConnectionTypeObserver {
        private static final int MAX_TRIES = 3;

        private final AuthTask<T> mAuthTask;
        private final AtomicInteger mNumTries;
        private final AtomicBoolean mIsTransientError;

        public static <T> void runAuthTask(AuthTask<T> authTask) {
            new ConnectionRetry<>(authTask).attempt();
        }

        private ConnectionRetry(AuthTask<T> authTask) {
            mAuthTask = authTask;
            mNumTries = new AtomicInteger(0);
            mIsTransientError = new AtomicBoolean(false);
        }

        /**
         * Tries running the {@link AuthTask} in the background. This object is never registered
         * as a {@link NetworkChangeNotifier.ConnectionTypeObserver} when this method is called.
         */
        private void attempt() {
            ThreadUtils.assertOnUiThread();
            // Clear any transient error.
            mIsTransientError.set(false);
            new AsyncTask<T>() {
                @Override
                public T doInBackground() {
                    try {
                        return mAuthTask.run();
                    } catch (AuthException ex) {
                        Log.w(TAG, "Failed to perform auth task: %s", ex.stringifyCausalChain());
                        Log.d(TAG, "Exception details:", ex);
                        mIsTransientError.set(ex.isTransientError());
                    }
                    return null;
                }
                @Override
                public void onPostExecute(T result) {
                    if (result != null) {
                        mAuthTask.onSuccess(result);
                    } else if (!mIsTransientError.get() || mNumTries.incrementAndGet() >= MAX_TRIES
                            || !NetworkChangeNotifier.isInitialized()) {
                        // Permanent error, ran out of tries, or we can't listen for network
                        // change events; give up.
                        mAuthTask.onFailure(mIsTransientError.get());
                    } else {
                        // Transient error with tries left; register for another attempt.
                        NetworkChangeNotifier.addConnectionTypeObserver(ConnectionRetry.this);
                    }
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        @Override
        public void onConnectionTypeChanged(int connectionType) {
            assert mNumTries.get() < MAX_TRIES;
            if (NetworkChangeNotifier.isOnline()) {
                // The network is back; stop listening and try again.
                NetworkChangeNotifier.removeConnectionTypeObserver(this);
                attempt();
            }
        }
    }

    @NativeMethods
    interface Natives {
        void onOAuth2TokenFetched(
                String authToken, boolean isTransientError, long nativeCallback);
        void reloadAllAccountsWithPrimaryAccountAfterSeeding(
                long nativeOAuth2TokenServiceDelegateAndroid, @Nullable String accountId);
    }
}
