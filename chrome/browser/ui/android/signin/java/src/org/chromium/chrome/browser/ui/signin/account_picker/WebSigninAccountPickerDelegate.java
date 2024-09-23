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
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.content_public.browser.LoadUrlParams;

/** Implementation of {@link AccountPickerDelegate} for the web-signin flow. */
public class WebSigninAccountPickerDelegate implements AccountPickerDelegate {
    private final Tab mCurrentTab;
    private final Profile mProfile;
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
        mProfile = currentTab.getProfile();
        mWebSigninBridgeFactory = webSigninBridgeFactory;
        mContinueUrl = continueUrl;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(mProfile);
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void onAccountPickerDestroy() {
        destroyWebSigninBridge();
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public boolean canHandleAddAccount() {
        return false;
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void addAccount() {
        // TODO(b/326019991): Remove this exception along with the delegate implementation once
        // all bottom sheet entry points will be started from `SigninAndHistorySyncActivity`.
        throw new UnsupportedOperationException(
                "WebSigninAccountPickerDelegate.addAccount() should never be called.");
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void signIn(CoreAccountInfo accountInfo, AccountPickerBottomSheetMediator mediator) {
        if (mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // In case an error is fired because cookies are taking longer to generate than usual,
            // if user retries the sign-in from the error screen, we need to sign out the user
            // first before signing in again.
            destroyWebSigninBridge();
            mSigninManager.signOut(SignoutReason.SIGNIN_RETRIGGERED_FROM_WEB_SIGNIN);
        }
        mWebSigninBridge =
                mWebSigninBridgeFactory.create(
                        mProfile,
                        accountInfo,
                        createWebSigninBridgeListener(mCurrentTab, mContinueUrl, mediator));
        mSigninManager.signin(
                accountInfo,
                SigninAccessPoint.WEB_SIGNIN,
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        // After the sign-in is finished in Chrome, we still need to wait for
                        // WebSigninBridge to be called to redirect to the continue url.
                    }

                    @Override
                    public void onSignInAborted() {
                        mediator.switchToTryAgainView();
                    }
                });
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void isAccountManaged(CoreAccountInfo accountInfo, Callback<Boolean> callback) {
        mSigninManager.isAccountManaged(accountInfo, callback);
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void setUserAcceptedAccountManagement(boolean confirmed) {
        mSigninManager.setUserAcceptedAccountManagement(confirmed);
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public String extractDomainName(String accountEmail) {
        return mSigninManager.extractDomainName(accountEmail);
    }

    private WebSigninBridge.Listener createWebSigninBridgeListener(
            Tab tab, String continueUrl, AccountPickerBottomSheetMediator mediator) {
        return new WebSigninBridge.Listener() {
            @MainThread
            @Override
            public void onSigninFailed(GoogleServiceAuthError error) {
                ThreadUtils.assertOnUiThread();
                @AccountConsistencyPromoAction int promoAction;
                if (error.getState() == State.INVALID_GAIA_CREDENTIALS) {
                    promoAction = AccountConsistencyPromoAction.AUTH_ERROR_SHOWN;
                    mediator.switchToAuthErrorView();
                } else {
                    promoAction = AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN;
                    mediator.switchToTryAgainView();
                }
                SigninMetricsUtils.logAccountConsistencyPromoAction(
                        promoAction, SigninAccessPoint.WEB_SIGNIN);
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
