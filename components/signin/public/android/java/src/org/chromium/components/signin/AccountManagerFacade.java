// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import com.google.common.base.Optional;

import org.chromium.base.Callback;

import java.util.List;

/**
 * Interface for {@link AccountManagerFacadeImpl}.
 */
public interface AccountManagerFacade {
    /**
     * Listener for {@link ChildAccountStatus.Status}.
     */
    interface ChildAccountStatusListener {
        /**
         * The method is called when child account status is ready.
         */
        void onStatusReady(@ChildAccountStatus.Status int status);
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
     * Returns whether the account cache has already been populated. {@link #tryGetGoogleAccounts()}
     * and similar methods will return instantly if the cache has been populated, otherwise these
     * methods may block waiting for the cache to be populated.
     */
    @AnyThread
    boolean isCachePopulated();

    /**
     * Retrieves all Google accounts on the device from the cache.
     * Returns an empty array if an error occurs while getting account list.
     * If the cache is not yet populated, the optional will be empty.
     */
    @AnyThread
    Optional<List<Account>> getGoogleAccounts();

    /**
     * Retrieves all Google accounts on the device.
     * Returns an empty array if an error occurs while getting account list.
     * This method is blocking, use {@link #getGoogleAccounts()} instead.
     */
    @AnyThread
    @Deprecated
    List<Account> tryGetGoogleAccounts();

    /**
     * Asynchronous version of {@link #getGoogleAccounts()}.
     */
    @MainThread
    void tryGetGoogleAccounts(final Callback<List<Account>> callback);

    /**
     * @return Whether or not there is an account authenticator for Google accounts.
     */
    @AnyThread
    boolean hasGoogleAccountAuthenticator();

    /**
     * Synchronously gets an OAuth2 access token. May return a cached version, use
     * {@link #invalidateAccessToken} to invalidate a token in the cache.
     * @param account The {@link Account} for which the token is requested.
     * @param scope OAuth2 scope for which the requested token should be valid.
     * @return The OAuth2 access token as an AccessTokenData with a string and an expiration time.
     */
    @WorkerThread
    AccessTokenData getAccessToken(Account account, String scope) throws AuthException;

    /**
     * Removes an OAuth2 access token from the cache with retries asynchronously.
     * Uses {@link #getAccessToken} to issue a new token after invalidating the old one.
     * @param accessToken The access token to invalidate.
     */
    @MainThread
    void invalidateAccessToken(String accessToken);

    /**
     * Checks the child account status of the given account.
     *
     * @param account The account to check the child account status.
     * @param listener The listener is called when the {@link ChildAccountStatus.Status} is ready.
     */
    @MainThread
    void checkChildAccountStatus(Account account, ChildAccountStatusListener listener);

    /**
     * Gets the boolean for whether the account is subject to minor mode restrictions.
     * If the result is not yet fetched, the optional will be empty.
     */
    Optional<Boolean> isAccountSubjectToMinorModeRestrictions(Account account);

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
     * Asks the user to enter a new password for an account, updating the saved credentials for
     * the account.
     */
    @MainThread
    void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback);

    /**
     * Gets profile data source.
     * @return {@link ProfileDataSource} if it is supported by implementation, null otherwise.
     */
    @MainThread
    @Nullable
    ProfileDataSource getProfileDataSource();

    /**
     * Returns the Gaia id for the account associated with the given email address.
     * If an account with the given email address is not installed on the device
     * then null is returned.
     *
     * This method will throw IllegalStateException if called on the main thread.
     *
     * @param accountEmail The email address of a Google account.
     */
    @WorkerThread
    @Nullable
    String getAccountGaiaId(String accountEmail);

    /**
     * Checks whether Google Play services is available.
     */
    @AnyThread
    boolean isGooglePlayServicesAvailable();
}
