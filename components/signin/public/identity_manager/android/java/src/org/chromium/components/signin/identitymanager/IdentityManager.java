// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.accounts.Account;
import android.support.annotation.MainThread;
import android.support.annotation.Nullable;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.OAuth2TokenService;

/**
 * IdentityManager provides access to native IdentityManager's public API to java components.
 */
public class IdentityManager {
    private static final String TAG = "IdentityManager";

    /**
     * IdentityManager.Observer is notified when the available account information are updated. This
     * is a subset of native's IdentityManager::Observer.
     */
    public interface Observer {
        /**
         * Called when an account becomes the user's primary account.
         * This method is not called during a reauth.
         */
        void onPrimaryAccountSet(CoreAccountInfo account);

        /**
         * Called when the user moves from having a primary account to no longer having a primary
         * account (note that the user may still have an *unconsented* primary account after this
         * event).
         */
        void onPrimaryAccountCleared(CoreAccountInfo account);
    }
    /**
     * A simple callback for getAccessToken.
     */
    public interface GetAccessTokenCallback extends OAuth2TokenService.GetAccessTokenCallback {}

    private long mNativeIdentityManager;
    private OAuth2TokenService mOAuth2TokenService;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Called by native to create an instance of IdentityManager.
     */
    @CalledByNative
    private static IdentityManager create(
            long nativeIdentityManager, OAuth2TokenService oAuth2TokenService) {
        assert nativeIdentityManager != 0;
        return new IdentityManager(nativeIdentityManager, oAuth2TokenService);
    }

    @VisibleForTesting
    public IdentityManager(long nativeIdentityManager, OAuth2TokenService oAuth2TokenService) {
        mNativeIdentityManager = nativeIdentityManager;
        mOAuth2TokenService = oAuth2TokenService;
    }

    /**
     * Called by native upon KeyedService's shutdown
     */
    @CalledByNative
    private void destroy() {
        mNativeIdentityManager = 0;
    }

    /**
     * Registers a IdentityManager.Observer
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Unregisters a IdentityManager.Observer
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies observers that the primary account was set in C++.
     */
    @CalledByNative
    private void onPrimaryAccountSet(CoreAccountInfo account) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountSet(account);
        }
    }

    /**
     * Notifies observers that the primary account was cleared in C++.
     */
    @CalledByNative
    @VisibleForTesting
    public void onPrimaryAccountCleared(CoreAccountInfo account) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountCleared(account);
        }
    }

    /**
     * Returns whether the user's primary account is available.
     */
    public boolean hasPrimaryAccount() {
        return IdentityManagerJni.get().hasPrimaryAccount(mNativeIdentityManager);
    }

    /**
     * Provides the information of all accounts that have refresh tokens.
     */
    @VisibleForTesting
    public CoreAccountInfo[] getAccountsWithRefreshTokens() {
        return IdentityManagerJni.get().getAccountsWithRefreshTokens(mNativeIdentityManager);
    }

    /**
     * Provides access to the core information of the user's primary account.
     * Returns null if no such info is available, either because there
     * is no primary account yet or because the user signed out.
     */
    public @Nullable CoreAccountInfo getPrimaryAccountInfo() {
        return IdentityManagerJni.get().getPrimaryAccountInfo(mNativeIdentityManager);
    }

    /**
     * Provides access to the account ID of the user's primary account. Returns null if no such info
     * is available.
     */
    public @Nullable CoreAccountId getPrimaryAccountId() {
        return IdentityManagerJni.get().getPrimaryAccountId(mNativeIdentityManager);
    }

    /**
     * Looks up and returns information for account with given |email_address|. If the account
     * cannot be found, return a null value.
     */
    public @Nullable CoreAccountInfo
    findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(String email) {
        return IdentityManagerJni.get()
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        mNativeIdentityManager, email);
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
        assert mOAuth2TokenService != null;
        // TODO(crbug.com/934688) The following should call a JNI method instead.
        mOAuth2TokenService.getAccessToken(account, scope, callback);
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
        // TODO(crbug.com/934688) The following should call a JNI method instead.
        OAuth2TokenService.getAccessTokenWithFacade(accountManagerFacade, account, scope, callback);
    }

    /**
     * Called by native to invalidate an OAuth2 token. Please note that the token is invalidated
     * asynchronously.
     */
    @MainThread
    public void invalidateAccessToken(String accessToken) {
        assert mOAuth2TokenService != null;

        // TODO(crbug.com/934688) The following should call a JNI method instead.
        mOAuth2TokenService.invalidateAccessToken(accessToken);
    }

    /**
     * Invalidates the old token (if non-null/non-empty) and asynchronously generates a new one.
     *
     * @deprecated Use invalidateAccessToken and getAccessToken instead. TODO(crbug.com/1002894):
     *         This method is needed by InvalidationClientService which is not necessary anymore.
     *
     * @param accountManagerFacade AccountManagerFacade to request the access token from.
     * @param account the account to get the access token for.
     * @param oldToken The old token to be invalidated or null.
     * @param scope The scope to get an auth token for (with Android-style 'oauth2:' prefix).
     * @param callback called on successful and unsuccessful fetching of auth token.
     */
    @Deprecated
    public static void getNewAccessTokenWithFacade(AccountManagerFacade accountManagerFacade,
            Account account, @Nullable String oldToken, String scope,
            GetAccessTokenCallback callback) {
        OAuth2TokenService.getNewAccessTokenWithFacade(
                accountManagerFacade, account, oldToken, scope, callback);
    }

    @NativeMethods
    interface Natives {
        public @Nullable CoreAccountInfo getPrimaryAccountInfo(long nativeIdentityManager);
        public @Nullable CoreAccountId getPrimaryAccountId(long nativeIdentityManager);
        public boolean hasPrimaryAccount(long nativeIdentityManager);
        public @Nullable CoreAccountInfo
        findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                long nativeIdentityManager, String email);
        public CoreAccountInfo[] getAccountsWithRefreshTokens(long nativeIdentityManager);
    }
}
