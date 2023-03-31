// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.content_public.browser.LoadUrlParams;

/** Implementation of {@link AccountPickerDelegate} for the web-signin flow. */
public class WebSigninAccountPickerDelegate implements AccountPickerDelegate {
    private final Tab mCurrentTab;
    private final WebSigninBridge.Factory mWebSigninBridgeFactory;
    private final String mContinueUrl;
    private final SigninManager mSigninManager;
    private final IdentityManager mIdentityManager;
    private @Nullable WebSigninBridge mWebSigninBridge;

    /**
     * @param currentTab The current tab where the account picker bottom sheet is displayed.
     * @param webSigninBridgeFactory A {@link WebSigninBridge.Factory} to create {@link
     *         WebSigninBridge} instances.
     * @param continueUrl The URL that the user would be redirected to after sign-in succeeds.
     */
    public WebSigninAccountPickerDelegate(
            Tab currentTab, WebSigninBridge.Factory webSigninBridgeFactory, String continueUrl) {
        mCurrentTab = currentTab;
        mWebSigninBridgeFactory = webSigninBridgeFactory;
        mContinueUrl = continueUrl;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
    }

    @Override
    public void destroy() {
        destroyWebSigninBridge();
    }

    @Override
    public void signIn(
            String accountEmail, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        if (mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // In case an error is fired because cookies are taking longer to generate than usual,
            // if user retries the sign-in from the error screen, we need to sign out the user
            // first before signing in again.
            destroyWebSigninBridge();
            mSigninManager.signOut(SignoutReason.SIGNIN_RETRIGGERED_FROM_WEB_SIGNIN);
        }
        AccountInfoServiceProvider.get().getAccountInfoByEmail(accountEmail).then(accountInfo -> {
            mWebSigninBridge =
                    mWebSigninBridgeFactory.create(Profile.getLastUsedRegularProfile(), accountInfo,
                            createWebSigninBridgeListener(
                                    mCurrentTab, mContinueUrl, onSignInErrorCallback));
            mSigninManager.signin(AccountUtils.createAccountFromName(accountEmail),
                    SigninAccessPoint.WEB_SIGNIN, new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            // After the sign-in is finished in Chrome, we still need to wait for
                            // WebSigninBridge to be called to redirect to the continue url.
                        }

                        @Override
                        public void onSignInAborted() {
                            WebSigninAccountPickerDelegate.this.destroyWebSigninBridge();
                        }
                    });
        });
    }

    @Override
    public @EntryPoint int getEntryPoint() {
        return EntryPoint.WEB_SIGNIN;
    }

    private static WebSigninBridge.Listener createWebSigninBridgeListener(
            Tab tab, String continueUrl, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        return new WebSigninBridge.Listener() {
            @MainThread
            @Override
            public void onSigninFailed(GoogleServiceAuthError error) {
                ThreadUtils.assertOnUiThread();
                onSignInErrorCallback.onResult(error);
            }

            @MainThread
            @Override
            public void onSigninSucceeded() {
                ThreadUtils.assertOnUiThread();
                if (tab.isDestroyed()) {
                    // This code path may be called asynchronously, assume that if the tab has been
                    // destroyed there is no point in continuing.
                    return;
                }
                tab.loadUrl(new LoadUrlParams(continueUrl));
            }
        };
    }

    private void destroyWebSigninBridge() {
        if (mWebSigninBridge != null) {
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }
    }
}
