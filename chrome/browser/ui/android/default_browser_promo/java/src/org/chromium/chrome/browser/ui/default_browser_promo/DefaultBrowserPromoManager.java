// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manage all types of default browser promo dialogs and listen to the activity state change to
 * trigger dialogs.
 */
@NullMarked
public class DefaultBrowserPromoManager {
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final DefaultBrowserPromoImpressionCounter mImpressionCounter;
    private final DefaultBrowserStateProvider mStateProvider;
    @Nullable private final Integer mSource;

    /**
     * @param activity Activity to show promo dialogs.
     * @param windowAndroid The {@link WindowAndroid} for sending an intent.
     * @param impressionCounter The {@link DefaultBrowserPromoImpressionCounter}
     * @param stateProvider The {@link DefaultBrowserStateProvider}
     * @param source The nullable source string for metrics (e.g. App Menu).
     */
    public DefaultBrowserPromoManager(
            Activity activity,
            WindowAndroid windowAndroid,
            DefaultBrowserPromoImpressionCounter impressionCounter,
            DefaultBrowserStateProvider stateProvider,
            @Nullable @DefaultBrowserPromoEntryPoint Integer source) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mImpressionCounter = impressionCounter;
        mStateProvider = stateProvider;
        mSource = source;
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
                    // After shown the Role Manager dialog, the default browser state may change,
                    // We need to fetch the default browser info again to make sure the state is
                    // updated before recording the outcome.
                    DefaultBrowserPromoUtils.getInstance()
                            .fetchDefaultBrowserInfo(
                                    result -> {
                                        recordOutcome(currentState);
                                    });
                },
                null);
    }

    /**
     * Records the outcome of a default browser promo interaction by comparing the browser's default
     * state before and after the user responded to the promo. Logs both general outcome metrics.
     *
     * @param oldState The {@link DefaultBrowserState} captured before the promo was shown.
     */
    private void recordOutcome(int oldState) {
        @DefaultBrowserState int newState = mStateProvider.getCurrentDefaultBrowserState();
        DefaultBrowserPromoMetrics.recordOutcome(
                oldState, newState, mImpressionCounter.getPromoCount());
        // Source (e.g. App Menu) specific metric.
        if (mSource != null) {
            DefaultBrowserPromoMetrics.recordOutcome(newState, mSource);
        }
    }
}
