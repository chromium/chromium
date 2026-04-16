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
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
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

    private final Profile mProfile;
    private final @Nullable TabModelSelector mTabModelSelector;
    private final WebSigninBridge.Factory mWebSigninBridgeFactory;
    private @Nullable WebSigninBridge mWebSigninBridge;
    // TODO(crbug.com/469772349): Remove deprecated constructor fields after activityless-signin
    // launch.
    private final @Nullable Tab mCurrentTab;
    private final @Nullable GURL mContinueUrl;

    /** Constructor for the activity-less sign-in flow. */
    public WebSigninAccountPickerDelegate(
            Profile profile,
            TabModelSelector tabModelSelector,
            WebSigninBridge.Factory webSigninBridgeFactory) {
        mProfile = profile;
        mTabModelSelector = tabModelSelector;
        mWebSigninBridgeFactory = webSigninBridgeFactory;
        mCurrentTab = null;
        mContinueUrl = null;
    }

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
        mTabModelSelector = null;
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

    /**
     * Implements {@link AccountPickerDelegate}.
     *
     * <p>TODO(crbug.com/469772349): Remove after activity-less sign-in launch.
     */
    @Override
    public void onSignInComplete(
            CoreAccountInfo accountInfo, AccountPickerDelegate.SigninStateController controller) {
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

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void runPostSigninAction(
            CoreAccountInfo signedInAccount,
            @Nullable DelegateContext delegateContext,
            Callback<@PostSigninOperationResult Integer> onComplete) {
        assert delegateContext != null;
        WebSigninDelegateContext webSigninDelegateContext =
                (WebSigninDelegateContext) delegateContext;

        // Destroy WebSigninBridge in case it is still alive to avoid interference with the new
        // sign-in.
        destroyWebSigninBridge();

        assert mTabModelSelector != null;
        @Nullable Tab resolvedTab =
                mTabModelSelector.getTabById(webSigninDelegateContext.getTabId());

        // Create WebSigninBridge and wait for redirect to the continue url.
        mWebSigninBridge =
                mWebSigninBridgeFactory.createWithCoreAccountId(
                        mProfile,
                        signedInAccount.getId(),
                        createWebSigninBridgeCallback(
                                resolvedTab,
                                webSigninDelegateContext.getContinueUrl(),
                                onComplete));
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
            @Nullable Tab tab,
            GURL continueUrl,
            Callback<@PostSigninOperationResult Integer> onComplete) {
        return (result) -> {
            ThreadUtils.assertOnUiThread();
            switch (result) {
                case WebSigninTrackerResult.SUCCESS:
                    onComplete.onResult(PostSigninOperationResult.SUCCESS);
                    if (tab != null && !tab.isDestroyed()) {
                        // This code path may be called asynchronously, so check
                        // that the tab is still alive.
                        tab.loadUrl(new LoadUrlParams(continueUrl));
                    }
                    SigninSurveyController.registerTrigger(
                            mProfile, SigninSurveyController.SigninSurveyType.WEB);
                    break;
                case WebSigninTrackerResult.AUTH_ERROR:
                    SigninMetricsUtils.logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.AUTH_ERROR_SHOWN,
                            SigninAccessPoint.WEB_SIGNIN);
                    onComplete.onResult(PostSigninOperationResult.AUTH_ERROR);
                    break;
                case WebSigninTrackerResult.OTHER_ERROR:
                    SigninMetricsUtils.logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN,
                            SigninAccessPoint.WEB_SIGNIN);
                    onComplete.onResult(PostSigninOperationResult.OTHER_ERROR);
                    break;
                default:
                    throw new IllegalStateException("Unexpected result: " + result);
            }
        };
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
