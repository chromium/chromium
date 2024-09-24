// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.List;

/** Interface for {@link AccountManagerFacadeImpl}. */
public interface AccountManagerFacade {
    /** A callback for getAccessToken. */
    interface GetAccessTokenCallback {
        /**
         * Invoked on the UI thread if a token is provided.
         *
         * @param token Access token, guaranteed not to be null.
         */
        void onGetTokenSuccess(AccessTokenData token);

        /**
         * Invoked on the UI thread if no token is available.
         *
         * @param isTransientError Indicates if the error is transient (network timeout or
         *     unavailable, etc) or persistent (bad credentials, permission denied, etc).
         */
        void onGetTokenFailure(boolean isTransientError);
    }

    // TODO(crbug.com/40201126): consider refactoring this interface to use Promises.
    /** Listener for whether the account is a child one. */
    interface ChildAccountStatusListener {
        /**
         * The method is called when the status of the account (whether it is a child one) is ready.
         *
         * @param isChild If account is a child account.
         * @param childAccount The child account if isChild != false; null otherwise.
         */
        void onStatusReady(boolean isChild, @Nullable CoreAccountInfo childAccount);
    }

    /**
     * Adds an observer to receive accounts change notifications.
     * @param observer the observer to add.
     */
    @MainThread
    void addObserver(AccountsChangeObserver observer);

    /**
     * Removes an observer that was previously added using {@link #addObserver}.
     * @param observer the observer to remove.
     */
    @MainThread
    void removeObserver(AccountsChangeObserver observer);

    /**
     * Retrieves corresponding {@link CoreAccountInfo}s for filtered accounts. The {@link Promise}
     * will be fulfilled once the accounts cache is populated and gaia ids are fetched. If an error
     * occurs while getting account list, the returned {@link Promise} will wrap an empty array.
     *
     * <p>Since a different {@link Promise} will be returned every time the accounts get updated,
     * this makes he {@link Promise}t a bad candidate for end users to cache locally unless the end
     * users are awaiting the {@link CoreAccountInfo}s for current list of accounts only.
     */
    @MainThread
    Promise<List<CoreAccountInfo>> getCoreAccountInfos();

    /**
     * Asynchronously gets OAuth2 access token for the given account and scope. May return a cached
     * version, use {@link #invalidateAccessToken} to invalidate a token in the cache. Please note
     * that this method expects a scope with 'oauth2:' prefix.
     *
     * @param account the account to get the access token for.
     * @param scope The scope to get an auth token for (with Android-style 'oauth2:' prefix).
     * @param callback called on successful and unsuccessful fetching of auth token.
     */
    @MainThread
    void getAccessToken(
            CoreAccountInfo coreAccountInfo, String scope, GetAccessTokenCallback callback);

    /**
     * Removes an OAuth2 access token from the cache with retries asynchronously. Uses {@link
     * #getAccessToken} to issue a new token after invalidating the old one.
     *
     * @param accessToken The access token to invalidate.
     * @param completedRunnable The callback to run after the operation is complete. Can be null.
     */
    @MainThread
    void invalidateAccessToken(String accessToken, @Nullable Runnable completedRunnable);

    /**
     * Wait for all pending token requests and invokes the passed callback. If there are no pending
     * requests, the callback is invoked immediately. Currently, can only be called once - the
     * behavior for subsequent calls is not specified.
     *
     * @param requestsCompletedCallback callback to call when all pending token requests complete.
     */
    @MainThread
    void waitForPendingTokenRequestsToComplete(Runnable requestsCompletedCallback);

    /**
     * Checks the child account status of the given account.
     *
     * @param coreAccountInfo The CoreAccountInfo to check the child account status.
     * @param listener The listener is called when the status of the account (whether it is a child
     *     one) is ready.
     */
    @MainThread
    void checkChildAccountStatus(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener);

    /**
     * @param coreAccountInfo The {@link CoreAccountInfo} used to look up capabilities.
     * @return account capabilities for the given account.
     */
    @MainThread
    Promise<AccountCapabilities> getAccountCapabilities(CoreAccountInfo coreAccountInfo);

    /**
     * Creates an intent that will ask the user to add a new account to the device. See
     * {@link AccountManager#addAccount} for details.
     * @param callback The callback to get the created intent. Will be invoked on the main
     *         thread. If there is an issue while creating the intent, callback will receive
     *         null.
     */
    @AnyThread
    void createAddAccountIntent(Callback<Intent> callback);

    /**
     * Asks the user to enter a new password for an account, updating the saved credentials for the
     * account.
     */
    @MainThread
    void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback);

    /**
     * Asks the user to confirm their knowledge of the password to the given account.
     *
     * @param account The {@link Account} to confirm the credentials for.
     * @param activity The {@link Activity} context to use for launching a new authenticator-defined
     *     sub-Activity to prompt the user to confirm the account's password.
     * @param callback The callback to indicate whether the user successfully confirmed their
     *     knowledge of the account's credentials.
     */
    @AnyThread
    void confirmCredentials(Account account, Activity activity, Callback<Bundle> callback);

    /** Whether fetching the list of accounts from the device eventually succeeded. */
    // TODO(crbug.com/330304719): Handle this with exceptions rather than a boolean.
    boolean didAccountFetchSucceed();

    /**
     * Used in live tests to prevent subsequent token requests from going through. After this method
     * is invoked - all subsequent calls to `getAccessToken` and `invalidateAccessToken` will
     * immediately fail. Should only be used in live tests.
     */
    @MainThread
    void disallowTokenRequestsForTesting();
}
