// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.os.Bundle;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.util.function.Function;

/**
 * Implementation of {@link AccountPickerDelegate} for the web-signin flow.
 *
 * <p>TODO(crbug.com/469772349): Remove {@link AccountPickerDelegate} in preference of {@link
 * BottomSheetSigninAndHistorySyncCoordinator.Delegate} to handle sign-in callbacks.
 * go/activityless-signin
 */
@NullMarked
public class WebSigninAccountPickerDelegate
        implements AccountPickerDelegate, BottomSheetSigninAndHistorySyncCoordinator.Delegate {

    private final WebSigninBridge.Factory mWebSigninBridgeFactory;
    private @Nullable WebSigninBridge mWebSigninBridge;
    // TODO(crbug.com/469772349): Remove deprecated constructor fields after activityless-signin
    // launch.
    private final @Nullable Profile mProfile;
    private final @Nullable Tab mCurrentTab;
    private final @Nullable GURL mContinueUrl;

    /** Constructor for the activity-less sign-in flow. */
    public WebSigninAccountPickerDelegate(WebSigninBridge.Factory webSigninBridgeFactory) {
        mWebSigninBridgeFactory = webSigninBridgeFactory;
        mProfile = null;
        mCurrentTab = null;
        mContinueUrl = null;
    }

    /**
     * TODO(crbug.com/478813952): Pass in TabModelSelector to fetch Tab from
     * WebSigninDelegateContext#mTabId. Support keeping the bottom sheet open while synchronized
     * cookies are being created. Handle WebSigninTrackerResult errors, and redirect to
     * WebSigninDelegateContext#mContinueUrl
     */

    /**
     * @deprecated Prefer {@link #WebSigninAccountPickerDelegate(WebSigninBridge.Factory)}.
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
        // TODO(crbug.com/469772349): Remove after activity-less sign-in launch.
        throw new UnsupportedOperationException(
                "WebSigninAccountPickerDelegate.addAccount() should never be called.");
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void onSignInComplete(
            CoreAccountInfo accountInfo, AccountPickerDelegate.SigninStateController controller) {
        assert mProfile != null;
        assert mCurrentTab != null;
        assert mContinueUrl != null;
        // Destroy WebSigninBridge in case it is still alive to avoid interference with the new
        // sign-in.
        destroyWebSigninBridge();
        // Create WebSigninBridge and wait for redirect to the continue url.
        mWebSigninBridge =
                mWebSigninBridgeFactory.createWithCoreAccountId(
                        mProfile,
                        accountInfo.getId(),
                        createWebSigninBridgeCallback(mCurrentTab, mContinueUrl, controller));
    }

    /**
     * Implements {@link AccountPickerDelegate} and {@link
     * BottomSheetSigninAndHistorySyncCoordinator.Delegate}.
     */
    @Override
    public @FlowVariant String getSigninFlowVariant() {
        return FlowVariant.WEB;
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public @Nullable Function<Bundle, DelegateContext> getDelegateContextFactory() {
        return WebSigninDelegateContext::fromBundle;
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
        };
    }

    private void destroyWebSigninBridge() {
        if (mWebSigninBridge != null) {
            mWebSigninBridge.destroy();
            mWebSigninBridge = null;
        }
    }
}
