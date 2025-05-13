// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

/**
 * Java instance for the native ProfileOAuth2TokenServiceDelegate.
 *
 * <p>This class forwards calls to request or invalidate access tokens made by native code to
 * AccountManagerFacade and forwards callbacks to native code.
 *
 * <p>
 */
@NullMarked
final class ProfileOAuth2TokenServiceDelegate {
    private static final String OAUTH2_SCOPE_PREFIX = "oauth2:";

    private final AccountManagerFacade mAccountManagerFacade;

    @VisibleForTesting
    @CalledByNative
    ProfileOAuth2TokenServiceDelegate(long nativeProfileOAuth2TokenServiceDelegate) {
        assert nativeProfileOAuth2TokenServiceDelegate != 0
                : "nativeProfileOAuth2TokenServiceDelegate should not be zero!";
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
    }

    /**
     * Called by native method AndroidAccessTokenFetcher::Start() to retrieve OAuth2 tokens.
     * @param accountEmail The account email.
     * @param scope The scope to get an auth token for (without Android-style 'oauth2:' prefix).
     * @param nativeCallback The pointer to the native callback that should be run upon
     *         completion.
     */
    @MainThread
    @CalledByNative
    private void getAccessTokenFromNative(
            String accountEmail, String scope, final long nativeCallback) {
        assert accountEmail != null : "Account email cannot be null!";
        mAccountManagerFacade
                .getAccounts()
                .then(
                        accounts -> {
                            final @Nullable CoreAccountInfo coreAccountInfo =
                                    AccountUtils.findAccountByEmail(accounts, accountEmail);
                            getAccessToken(coreAccountInfo, scope, nativeCallback);
                        });
    }

    private void getAccessToken(
            @Nullable CoreAccountInfo coreAccountInfo, String scope, final long nativeCallback) {
        if (coreAccountInfo == null) {
            ThreadUtils.postOnUiThread(
                    () -> {
                        ProfileOAuth2TokenServiceDelegateJni.get()
                                .onOAuth2TokenFetched(
                                        null,
                                        AccessTokenData.NO_KNOWN_EXPIRATION_TIME,
                                        new GoogleServiceAuthError(
                                                GoogleServiceAuthErrorState.REQUEST_CANCELED),
                                        nativeCallback);
                    });
            return;
        }
        String oauth2Scope = OAUTH2_SCOPE_PREFIX + scope;
        mAccountManagerFacade.getAccessToken(
                coreAccountInfo,
                oauth2Scope,
                new AccountManagerFacade.GetAccessTokenCallback() {
                    @Override
                    public void onGetTokenSuccess(AccessTokenData token) {
                        ProfileOAuth2TokenServiceDelegateJni.get()
                                .onOAuth2TokenFetched(
                                        token.getToken(),
                                        token.getExpirationTimeSecs(),
                                        new GoogleServiceAuthError(
                                                GoogleServiceAuthErrorState.NONE),
                                        nativeCallback);
                    }

                    @Override
                    public void onGetTokenFailure(GoogleServiceAuthError authError) {
                        ProfileOAuth2TokenServiceDelegateJni.get()
                                .onOAuth2TokenFetched(
                                        null,
                                        AccessTokenData.NO_KNOWN_EXPIRATION_TIME,
                                        authError,
                                        nativeCallback);
                    }
                });
    }

    /**
     * Called by native to invalidate an OAuth2 token. Please note that the token is invalidated
     * asynchronously.
     */
    @MainThread
    @CalledByNative
    void invalidateAccessToken(String accessToken) {
        // TODO(https://crbug.com/40637583): Pass a callback from native to wait for completion.
        mAccountManagerFacade.invalidateAccessToken(accessToken, null);
    }

    /**
     * Called by the native method ProfileOAuth2TokenServiceDelegate::RefreshTokenIsAvailable to
     * check whether the account has an OAuth2 refresh token. TODO(crbug.com/40928950): Use
     * CoreAccountId instead of string email.
     */
    @VisibleForTesting
    @CalledByNative
    boolean hasOAuth2RefreshToken(String accountEmail) {
        var promise = mAccountManagerFacade.getAccounts();
        return promise.isFulfilled()
                && AccountUtils.findAccountByEmail(promise.getResult(), accountEmail) != null;
    }

    @NativeMethods
    interface Natives {
        /**
         * Called to C++ when fetching of an OAuth2 token is finished.
         *
         * @param authToken The string value of the OAuth2 token.
         * @param expirationTimeSecs The number of seconds after the Unix epoch when the token is
         *     scheduled to expire. It is set to 0 if there's no known expiration time.
         * @param authError The {@link GoogleServiceAuthError} encountered during token fetch. Not
         *     checked if authToken is not null.
         * @param nativeCallback the pointer to the native callback that should be run upon
         *     completion.
         */
        void onOAuth2TokenFetched(
                @Nullable String authToken,
                long expirationTimeSecs,
                @JniType("GoogleServiceAuthError") GoogleServiceAuthError authError,
                long nativeCallback);
    }
}
