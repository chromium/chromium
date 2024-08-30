// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manage all types of default browser promo dialogs and listen to the activity state change to
 * trigger dialogs.
 */
public class DefaultBrowserPromoManager {
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final DefaultBrowserPromoImpressionCounter mImpressionCounter;
    private final DefaultBrowserStateProvider mStateProvider;

    /**
     * @param activity Activity to show promo dialogs.
     * @param windowAndroid The {@link WindowAndroid} for sending an intent.
     * @param impressionCounter The {@link DefaultBrowserPromoImpressionCounter}
     * @param stateProvider The {@link DefaultBrowserStateProvider}
     */
    public DefaultBrowserPromoManager(
            Activity activity,
            WindowAndroid windowAndroid,
            DefaultBrowserPromoImpressionCounter impressionCounter,
            DefaultBrowserStateProvider stateProvider) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mImpressionCounter = impressionCounter;
        mStateProvider = stateProvider;
    }

    @SuppressLint({"WrongConstant", "NewApi"})
    void promoByRoleManager() {
        RoleManager roleManager = (RoleManager) mActivity.getSystemService(Context.ROLE_SERVICE);

        @DefaultBrowserState int currentState = mStateProvider.getCurrentDefaultBrowserState();
        DefaultBrowserPromoMetrics.recordRoleManagerShow(currentState);

        Intent intent = roleManager.createRequestRoleIntent(RoleManager.ROLE_BROWSER);
        mWindowAndroid.showCancelableIntent(
                intent,
                (resultCode, data) -> {
                    DefaultBrowserPromoMetrics.recordOutcome(
                            currentState,
                            mStateProvider.getCurrentDefaultBrowserState(),
                            mImpressionCounter.getPromoCount());
                },
                null);
    }
}
