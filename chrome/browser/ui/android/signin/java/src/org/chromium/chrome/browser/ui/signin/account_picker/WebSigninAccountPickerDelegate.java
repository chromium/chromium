// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.content_public.browser.LoadUrlParams;

/** Implementation of {@link AccountPickerDelegate} for the web-signin flow. */
@NullMarked
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
        mSigninManager = assumeNonNull(IdentityServicesProvider.get().getSigninManager(mProfile));
        mIdentityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile));
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
            // Destroy WebSigninBridge in case it is still alive to avoid interference with the new
            // sign-in.
            destroyWebSigninBridge();
            mSigninManager.signOut(SignoutReason.SIGNIN_RETRIGGERED_FROM_WEB_SIGNIN);
        }
        if (!SigninFeatureMap.isEnabled(SigninFeatures.DEFER_WEB_SIGNIN_TRACKER_CREATION)) {
            mWebSigninBridge =
                    mWebSigninBridgeFactory.create(
                            mProfile,
                            accountInfo,
                            createWebSigninBridgeCallback(mCurrentTab, mContinueUrl, mediator));
        }
        mSigninManager.signin(
                accountInfo,
                SigninAccessPoint.WEB_SIGNIN,
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        // Create WebSigninBridge and wait for redirect to the continue url.
                        if (SigninFeatureMap.isEnabled(
                                SigninFeatures.DEFER_WEB_SIGNIN_TRACKER_CREATION)) {
                            mWebSigninBridge =
                                    mWebSigninBridgeFactory.create(
                                            mProfile,
                                            accountInfo,
                                            createWebSigninBridgeCallback(
                                                    mCurrentTab, mContinueUrl, mediator));
                        }
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

    private Callback<@WebSigninTrackerResult Integer> createWebSigninBridgeCallback(
            Tab tab, String continueUrl, AccountPickerBottomSheetMediator mediator) {
        return (result) -> {
            ThreadUtils.assertOnUiThread();
            switch (result) {
                case WebSigninTrackerResult.SUCCESS:
                    if (!tab.isDestroyed()) {
                        // This code path may be called asynchronously, so check that the tab is
                        // still alive.
                        tab.loadUrl(new LoadUrlParams(continueUrl));
                    }
                    break;
                case WebSigninTrackerResult.AUTH_ERROR:
                    SigninMetricsUtils.logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.AUTH_ERROR_SHOWN,
                            SigninAccessPoint.WEB_SIGNIN);
                    mediator.switchToAuthErrorView();
                    break;
                case WebSigninTrackerResult.OTHER_ERROR:
                    SigninMetricsUtils.logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN,
                            SigninAccessPoint.WEB_SIGNIN);
                    mediator.switchToTryAgainView();
                    break;
            }
            destroyWebSigninBridge();
        };
    }

    private void destroyWebSigninBridge() {
        if (mWebSigninBridge != null) {
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }
    }
}
