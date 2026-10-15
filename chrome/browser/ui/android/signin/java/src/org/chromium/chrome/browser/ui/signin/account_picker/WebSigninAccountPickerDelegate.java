// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.os.Bundle;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.WebSigninTrackerBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

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
    private final SigninCookieWaiter mCookieWaiter;
    // TODO(crbug.com/469772349): Remove deprecated constructor fields after activityless-signin
    // launch.
    private final @Nullable Tab mCurrentTab;
    private final @Nullable GURL mContinueUrl;

    /** Constructor for the activity-less sign-in flow. */
    public WebSigninAccountPickerDelegate(
            Profile profile,
            TabModelSelector tabModelSelector,
            WebSigninTrackerBridge.Factory webSigninTrackerBridgeFactory) {
        mProfile = profile;
        mTabModelSelector = tabModelSelector;
        mCookieWaiter =
                new SigninCookieWaiter(
                        profile, webSigninTrackerBridgeFactory, SigninAccessPoint.WEB_SIGNIN);
        mCurrentTab = null;
        mContinueUrl = null;
    }

    /**
     * @deprecated Prefer {@link #WebSigninAccountPickerDelegate(WebSigninTrackerBridge.Factory)}.
     * @param currentTab The current tab where the account picker bottom sheet is displayed.
     * @param webSigninTrackerBridgeFactory A {@link WebSigninTrackerBridge.Factory} to create
     *     {@link WebSigninTrackerBridge} instances.
     * @param continueUrl The URL that the user would be redirected to after sign-in succeeds.
     */
    public WebSigninAccountPickerDelegate(
            Tab currentTab,
            WebSigninTrackerBridge.Factory webSigninTrackerBridgeFactory,
            GURL continueUrl) {
        mCurrentTab = currentTab;
        mProfile = currentTab.getProfile();
        mCookieWaiter =
                new SigninCookieWaiter(
                        mProfile, webSigninTrackerBridgeFactory, SigninAccessPoint.WEB_SIGNIN);
        mContinueUrl = continueUrl;
        mTabModelSelector = null;
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void onAccountPickerDestroy() {
        mCookieWaiter.cancel();
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
        Tab tab = mCurrentTab;
        GURL continueUrl = mContinueUrl;
        // Wait for the account to be available in cookies, then redirect to the continue url.
        mCookieWaiter.waitForCookies(
                accountInfo.getId(),
                (result) -> {
                    switch (result) {
                        case PostSigninOperationResult.SUCCESS:
                            controller.onSigninComplete();
                            if (!tab.isDestroyed()) {
                                // This code path may be called asynchronously, so check that the
                                // tab is still alive.
                                tab.loadUrl(createLoadUrlParamsForContinueUrl(continueUrl));
                            }
                            break;
                        case PostSigninOperationResult.AUTH_ERROR:
                            controller.showAuthError();
                            break;
                        case PostSigninOperationResult.OTHER_ERROR:
                            controller.showGenericError();
                            break;
                    }
                });
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void runPostSigninAction(
            CoreAccountInfo signedInAccount,
            @Nullable DelegateContext delegateContext,
            Callback<@PostSigninOperationResult Integer> onComplete) {
        assert delegateContext != null;
        SigninDelegateContext signinDelegateContext = (SigninDelegateContext) delegateContext;

        assert mTabModelSelector != null;
        @Nullable Tab resolvedTab = mTabModelSelector.getTabById(signinDelegateContext.getTabId());
        GURL continueUrl = signinDelegateContext.getContinueUrl();

        // Wait for the account to be available in cookies, then redirect to the continue url.
        mCookieWaiter.waitForCookies(
                signedInAccount.getId(),
                (result) -> {
                    onComplete.onResult(result);
                    if (result != PostSigninOperationResult.SUCCESS) {
                        return;
                    }
                    if (resolvedTab != null && !resolvedTab.isDestroyed()) {
                        // This code path may be called asynchronously, so check that the tab is
                        // still alive.
                        resolvedTab.loadUrl(createLoadUrlParamsForContinueUrl(continueUrl));
                    }
                    SigninSurveyController.registerTrigger(
                            mProfile, SigninSurveyController.SigninSurveyType.WEB);
                });
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
        return SigninDelegateContext::fromBundle;
    }

    private static LoadUrlParams createLoadUrlParamsForContinueUrl(GURL continueUrl) {
        LoadUrlParams params = new LoadUrlParams(continueUrl);
        // Mark the navigation as renderer-initiated rather than browser-initiated as the continue
        // URL originates from the web page.
        params.setIsRendererInitiated(true);
        // Use an opaque origin because the source cannot be trusted and should not have same-origin
        // privileges.
        params.setInitiatorOrigin(Origin.createOpaqueOrigin());
        return params;
    }
}
