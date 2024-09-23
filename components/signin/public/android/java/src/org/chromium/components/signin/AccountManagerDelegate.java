// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Abstraction of account management implementation.
 * Provides methods for getting accounts and managing auth tokens.
 */
public interface AccountManagerDelegate {
    /** Response code of the {@link AccountManagerDelegate#hasCapability} result. */
    @IntDef({CapabilityResponse.EXCEPTION, CapabilityResponse.YES, CapabilityResponse.NO})
    @Retention(RetentionPolicy.SOURCE)
    @interface CapabilityResponse {
        /** This value is returned when no valid response YES or NO is fetched from the server. */
        int EXCEPTION = 0;

        int YES = 1;
        int NO = 2;
    }

    /**
     * Attaches the {@link AccountsChangeObserver} to the delegate and registers the
     * accounts change receivers to listen to the accounts change broadcast from the
     * system.
     */
    @MainThread
    void attachAccountsChangeObserver(AccountsChangeObserver observer);

    /** Get all the accounts on device synchronously. */
    @WorkerThread
    Account[] getAccountsSynchronous() throws AccountManagerDelegateException;

    /**
     * Get an auth token.
     *
     * @param account The {@link Account} for which the auth token is requested.
     * @param authTokenScope The scope of the authToken being requested.
     * @return The access token data fetched from the authenticator.
     * @throws AuthException Indicates a failure in fetching the auth token perhaps due to a
     *     transient error or when user intervention is required (like confirming the credentials)
     *     which is expressed as an {@link Intent} to the handler. TODO(crbug.com/40745233): Rename
     *     this method to getAccessToken.
     */
    @WorkerThread
    AccessTokenData getAuthToken(Account account, String authTokenScope) throws AuthException;

    /**
     * @param authToken The auth token to invalidate.
     * @throws AuthException Indicates a failure clearing the auth token; can be transient.
     *     TODO(crbug.com/40745233): Rename this method to invalidateAccessToken.
     */
    @WorkerThread
    void invalidateAuthToken(String authToken) throws AuthException;

    /** Check whether the given account has a specific feature. */
    @WorkerThread
    boolean hasFeature(Account account, String feature);

    /**
     * Returns a {@link CapabilityResponse} which indicates whether the account has the requested
     * capability or has exception.
     */
    @WorkerThread
    @CapabilityResponse
    int hasCapability(Account account, String capability);

    /**
     * Creates an intent that will ask the user to add a new account to the device. See
     * {@link AccountManager#addAccount} for details.
     * @param callback The callback to get the created intent. Will be invoked on the main thread.
     *         If there is an issue while creating the intent, callback will receive null.
     */
    @AnyThread
    void createAddAccountIntent(Callback<Intent> callback);

    /**
     * Asks the user to enter a new password for an account, updating the saved credentials for the
     * account.
     * @param account The {@link Account} for which the update is requested.
     * @param activity The {@link Activity} context to use for launching a new authenticator-defined
     * sub-Activity to prompt the user to enter a password.
     * @param callback The callback to indicate whether update is succeed or not.
     */
    @AnyThread
    void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback);

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
     * Asks the user to confirm their knowledge of the password to the given account.
     *
     * @param account The {@link Account} to confirm the credentials for.
     * @param activity The {@link Activity} context to use for launching a new authenticator-defined
     *                 sub-Activity to prompt the user to confirm the account's password.
     * @param callback The callback to indicate whether the user successfully confirmed their
     *                 knowledge of the account's credentials.
     */
    void confirmCredentials(Account account, Activity activity, Callback<Bundle> callback);
}
