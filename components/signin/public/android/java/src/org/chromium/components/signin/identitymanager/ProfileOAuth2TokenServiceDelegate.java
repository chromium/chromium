// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.List;

/**
 * Java instance for the native ProfileOAuth2TokenServiceDelegate.
 *
 * <p>This class forwards calls to request or invalidate access tokens made by native code to
 * AccountManagerFacade and forwards callbacks to native code.
 *
 * <p>
 */
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
                .getCoreAccountInfos()
                .then(
                        coreAccountInfos -> {
                            final CoreAccountInfo coreAccountInfo =
                                    AccountUtils.findCoreAccountInfoByEmail(
                                            coreAccountInfos, accountEmail);
                            if (coreAccountInfo == null) {
                                ThreadUtils.postOnUiThread(
                                        () -> {
                                            ProfileOAuth2TokenServiceDelegateJni.get()
                                                    .onOAuth2TokenFetched(
                                                            null,
                                                            AccessTokenData
                                                                    .NO_KNOWN_EXPIRATION_TIME,
                                                            false,
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
                                                            false,
                                                            nativeCallback);
                                        }

                                        @Override
                                        public void onGetTokenFailure(boolean isTransientError) {
                                            ProfileOAuth2TokenServiceDelegateJni.get()
                                                    .onOAuth2TokenFetched(
                                                            null,
                                                            AccessTokenData
                                                                    .NO_KNOWN_EXPIRATION_TIME,
                                                            isTransientError,
                                                            nativeCallback);
                                        }
                                    });
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
        Promise<List<CoreAccountInfo>> promise = mAccountManagerFacade.getCoreAccountInfos();
        return promise.isFulfilled()
                && AccountUtils.findCoreAccountInfoByEmail(promise.getResult(), accountEmail)
                        != null;
    }

    @NativeMethods
    interface Natives {
        /**
         * Called to C++ when fetching of an OAuth2 token is finished.
         *
         * @param authToken The string value of the OAuth2 token.
         * @param expirationTimeSecs The number of seconds after the Unix epoch when the token is
         *     scheduled to expire. It is set to 0 if there's no known expiration time.
         * @param isTransientError Indicates if the error is transient (network timeout or *
         *     unavailable, etc) or persistent (bad credentials, permission denied, etc).
         * @param nativeCallback the pointer to the native callback that should be run upon
         *     completion.
         */
        void onOAuth2TokenFetched(
                String authToken,
                long expirationTimeSecs,
                boolean isTransientError,
                long nativeCallback);
    }
}
