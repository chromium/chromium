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

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;

import java.util.List;

/** Interface for {@link AccountManagerFacadeImpl}. */
@NullMarked
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
         * @param authError The {@link GoogleServiceAuthError} encountered during token fetch.
         */
        void onGetTokenFailure(GoogleServiceAuthError authError);
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
     *
     * @param observer the observer to remove.
     */
    @MainThread
    void removeObserver(AccountsChangeObserver observer);

    /**
     * Retrieves corresponding {@link AccountInfo}s for filtered accounts. The {@link Promise} will
     * be fulfilled once the accounts cache is populated and gaia ids are fetched. If an error
     * occurs while getting account list, the returned {@link Promise} will wrap an empty list.
     *
     * <p>Since a different {@link Promise} will be returned every time the accounts get updated,
     * this makes the {@link Promise} a bad candidate for end users to cache locally.
     */
    @MainThread
    Promise<List<AccountInfo>> getAccounts();

    /**
     * Asynchronously gets OAuth2 access token for the given account and scope. May return a cached
     * version, use {@link #invalidateAccessToken} to invalidate a token in the cache.
     *
     * @param account the account to get the access token for.
     * @param scope The scope to get an auth token for.
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
     * Check whether the account is subject to parental controls.
     *
     * @param coreAccountInfo The CoreAccountInfo to check is subject to parental controls.
     * @param listener The listener is called when the status of the account (whether it is subject
     *     to parental controls) is ready.
     */
    @MainThread
    void checkIsSubjectToParentalControls(
            CoreAccountInfo coreAccountInfo, ChildAccountStatusListener listener);

    /**
     * @param coreAccountInfo The {@link CoreAccountInfo} used to look up capabilities.
     * @return account capabilities for the given account.
     */
    @MainThread
    Promise<AccountCapabilities> getAccountCapabilities(CoreAccountInfo coreAccountInfo);

    /**
     * Creates an intent that will ask the user to add a new account to the device. See {@link
     * AccountManager#addAccount} for details.
     *
     * @param prefilledEmail The email address to prefill in the add account flow, or null if no
     *     email should be prefilled.
     * @param callback The callback to get the created intent. Will be invoked on the main thread.
     *     If there is an issue while creating the intent, callback will receive null.
     */
    @AnyThread
    void createAddAccountIntent(
            @Nullable String prefilledEmail, Callback<@Nullable Intent> callback);

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
    void confirmCredentials(
            Account account, @Nullable Activity activity, Callback<@Nullable Bundle> callback);

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
