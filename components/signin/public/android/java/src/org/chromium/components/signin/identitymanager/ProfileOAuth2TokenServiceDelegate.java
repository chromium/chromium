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
import org.chromium.google_apis.gaia.CoreAccountId;
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
    private final long mNativePtr;
    private final AccountManagerFacade mAccountManagerFacade;

    @VisibleForTesting
    @CalledByNative
    ProfileOAuth2TokenServiceDelegate(long nativeProfileOAuth2TokenServiceDelegate) {
        assert nativeProfileOAuth2TokenServiceDelegate != 0
                : "nativeProfileOAuth2TokenServiceDelegate should not be zero!";
        mNativePtr = nativeProfileOAuth2TokenServiceDelegate;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
    }

    /**
     * Called by native method AndroidAccessTokenFetcher::Start() to retrieve OAuth2 tokens.
     *
     * @param coreAccountInfo The account info.
     * @param scope The scope to get an auth token for.
     * @param nativeCallback The pointer to the native callback that should be run upon completion.
     */
    @MainThread
    @CalledByNative
    private void getAccessTokenFromNative(
            @Nullable @JniType("CoreAccountInfo") CoreAccountInfo coreAccountInfo,
            String scope,
            final long nativeCallback) {
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
        mAccountManagerFacade.getAccessToken(
                coreAccountInfo,
                scope,
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
     * check whether the account has an OAuth2 refresh token.
     */
    @VisibleForTesting
    @CalledByNative
    boolean hasOAuth2RefreshToken(@JniType("CoreAccountId") CoreAccountId coreAccountId) {
        var promise = mAccountManagerFacade.getAccounts();
        return promise.isFulfilled()
                && AccountUtils.findAccountByGaiaId(promise.getResult(), coreAccountId.getId())
                        != null;
    }

    @MainThread
    void updateAuthErrorForTesting(CoreAccountId accountId, GoogleServiceAuthError authError) {
        ProfileOAuth2TokenServiceDelegateJni.get()
                .updateAuthErrorFromJava(
                        mNativePtr, accountId, authError, /* fireAuthErrorChanged= */ false);
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

        /**
         * Called to C++ to update auth error.
         *
         * @param accountId The account which has the auth error.
         * @param authError The {@link GoogleServiceAuthError} to set for the account.
         * @param fireAuthErrorChanged Whether observers should be notified of this update.
         */
        void updateAuthErrorFromJava(
                long nativeProfileOAuth2TokenServiceDelegateAndroid,
                @JniType("CoreAccountId") CoreAccountId accountId,
                @JniType("GoogleServiceAuthError") GoogleServiceAuthError authError,
                boolean fireAuthErrorChanged);
    }
}
