// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/** Implementation of {@link AccountPickerDelegate} for the web-signin flow. */
@NullMarked
public class WebSigninAccountPickerDelegate implements AccountPickerDelegate {
    private final Tab mCurrentTab;
    private final Profile mProfile;
    private final WebSigninBridge.Factory mWebSigninBridgeFactory;
    private final GURL mContinueUrl;
    private @Nullable WebSigninBridge mWebSigninBridge;

    /**
     * @param currentTab The current tab where the account picker bottom sheet is displayed.
     * @param webSigninBridgeFactory A {@link WebSigninBridge.Factory} to create {@link
     *     WebSigninBridge} instances.
     * @param continueUrl The URL that the user would be redirected to after sign-in succeeds.
     */
    public WebSigninAccountPickerDelegate(
            Tab currentTab, WebSigninBridge.Factory webSigninBridgeFactory, GURL continueUrl) {
        mCurrentTab = currentTab;
        mProfile = currentTab.getProfile();
        mWebSigninBridgeFactory = webSigninBridgeFactory;
        mContinueUrl = continueUrl;
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
    public void onSignoutBeforeSignin() {
        // Destroy WebSigninBridge in case it is still alive to avoid interference with the new
        // sign-in.
        destroyWebSigninBridge();
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void onSignInComplete(
            CoreAccountInfo accountInfo, AccountPickerDelegate.SigninStateController controller) {
        // Create WebSigninBridge and wait for redirect to the continue url.
        mWebSigninBridge =
                mWebSigninBridgeFactory.create(
                        mProfile,
                        accountInfo,
                        createWebSigninBridgeCallback(mCurrentTab, mContinueUrl, controller));
    }

    private Callback<@WebSigninTrackerResult Integer> createWebSigninBridgeCallback(
            Tab tab, GURL continueUrl, AccountPickerDelegate.SigninStateController controller) {
        return (result) -> {
            ThreadUtils.assertOnUiThread();
            switch (result) {
                case WebSigninTrackerResult.SUCCESS:
                    controller.onSigninComplete();
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
                    controller.showAuthError();
                    break;
                case WebSigninTrackerResult.OTHER_ERROR:
                    SigninMetricsUtils.logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN,
                            SigninAccessPoint.WEB_SIGNIN);
                    controller.showGenericError();
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

    @Override
    public @FlowVariant String getSigninFlowVariant() {
        return FlowVariant.WEB;
    }
}
