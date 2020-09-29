// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.accounts.Account;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;

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

        /**
         * Called when the Gaia cookie has been deleted explicitly by a user action, e.g. from
         * the settings.
         */
        default void onAccountsCookieDeletedByUserAction() {}

        /**
         * Called after an account is updated.
         */
        default void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {}
    }
    /**
     * A simple callback for getAccessToken.
     */
    public interface GetAccessTokenCallback
            extends ProfileOAuth2TokenServiceDelegate.GetAccessTokenCallback {}

    private long mNativeIdentityManager;
    private ProfileOAuth2TokenServiceDelegate mProfileOAuth2TokenServiceDelegate;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Called by native to create an instance of IdentityManager.
     */
    @CalledByNative
    private static IdentityManager create(long nativeIdentityManager,
            ProfileOAuth2TokenServiceDelegate profileOAuth2TokenServiceDelegate) {
        assert nativeIdentityManager != 0;
        return new IdentityManager(nativeIdentityManager, profileOAuth2TokenServiceDelegate);
    }

    @VisibleForTesting
    public IdentityManager(long nativeIdentityManager,
            ProfileOAuth2TokenServiceDelegate profileOAuth2TokenServiceDelegate) {
        mNativeIdentityManager = nativeIdentityManager;
        mProfileOAuth2TokenServiceDelegate = profileOAuth2TokenServiceDelegate;
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

    @CalledByNative
    @VisibleForTesting
    public void onAccountsCookieDeletedByUserAction() {
        for (Observer observer : mObservers) {
            observer.onAccountsCookieDeletedByUserAction();
        }
    }

    /**
     * Called after an account is updated.
     */
    @CalledByNative
    private void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        for (Observer observer : mObservers) {
            observer.onExtendedAccountInfoUpdated(accountInfo);
        }
    }

    /**
     * Returns whether the user's primary account is available.
     */
    public boolean hasPrimaryAccount() {
        return getPrimaryAccountInfo(ConsentLevel.SYNC) != null;
    }

    /**
     * Provides the information of all accounts that have refresh tokens.
     */
    @VisibleForTesting
    public CoreAccountInfo[] getAccountsWithRefreshTokens() {
        return IdentityManagerJni.get().getAccountsWithRefreshTokens(mNativeIdentityManager);
    }

    // TODO(https://crbug.com/1046746): Remove this after migrating internal usages.
    /** @deprecated Use {@link #getPrimaryAccountInfo(int)} instead. */
    @Deprecated
    public @Nullable CoreAccountInfo getPrimaryAccountInfo() {
        return getPrimaryAccountInfo(ConsentLevel.SYNC);
    }

    /**
     * Provides access to the core information of the user's primary account.
     * Returns non-null if the primary account was set AND the required consent level was granted,
     * null otherwise.
     *
     * @param consentLevel {@link ConsentLevel} necessary for the caller. Most features should use
     *         {@link ConsentLevel.SYNC}.
     */
    public @Nullable CoreAccountInfo getPrimaryAccountInfo(@ConsentLevel int consentLevel) {
        return IdentityManagerJni.get().getPrimaryAccountInfo(mNativeIdentityManager, consentLevel);
    }

    /**
     * Looks up and returns information for account with given |email|. If the account
     * cannot be found, return a null value.
     */
    public @Nullable AccountInfo findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
            String email) {
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
        assert mProfileOAuth2TokenServiceDelegate != null;
        // TODO(crbug.com/934688) The following should call a JNI method instead.
        mProfileOAuth2TokenServiceDelegate.getAccessToken(account, scope, callback);
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
        ProfileOAuth2TokenServiceDelegate.getAccessTokenWithFacade(
                accountManagerFacade, account, scope, callback);
    }

    /**
     * Called by native to invalidate an OAuth2 token. Please note that the token is invalidated
     * asynchronously.
     */
    @MainThread
    public void invalidateAccessToken(String accessToken) {
        assert mProfileOAuth2TokenServiceDelegate != null;

        // TODO(crbug.com/934688) The following should call a JNI method instead.
        mProfileOAuth2TokenServiceDelegate.invalidateAccessToken(accessToken);
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
        ProfileOAuth2TokenServiceDelegate.getNewAccessTokenWithFacade(
                accountManagerFacade, account, oldToken, scope, callback);
    }

    @NativeMethods
    public interface Natives {
        @Nullable
        CoreAccountInfo getPrimaryAccountInfo(long nativeIdentityManager, int consentLevel);
        @Nullable
        AccountInfo findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                long nativeIdentityManager, String email);
        CoreAccountInfo[] getAccountsWithRefreshTokens(long nativeIdentityManager);
    }
}
