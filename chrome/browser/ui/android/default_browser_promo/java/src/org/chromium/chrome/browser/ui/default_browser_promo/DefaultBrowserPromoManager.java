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
    private final @DefaultBrowserState int mCurrentState;
    private final WindowAndroid mWindowAndroid;

    /**
     * @param activity Activity to show promo dialogs.
     * @param windowAndroid The {@link WindowAndroid} for sending an intent.
     * @param currentState The current {@link DefaultBrowserState} in the system.
     */
    public DefaultBrowserPromoManager(
            Activity activity, WindowAndroid windowAndroid, @DefaultBrowserState int currentState) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mCurrentState = currentState;
    }

    @SuppressLint({"WrongConstant", "NewApi"})
    void promoByRoleManager() {
        RoleManager roleManager = (RoleManager) mActivity.getSystemService(Context.ROLE_SERVICE);

        DefaultBrowserPromoMetrics.recordRoleManagerShow(mCurrentState);

        Intent intent = roleManager.createRequestRoleIntent(RoleManager.ROLE_BROWSER);
        mWindowAndroid.showCancelableIntent(
                intent,
                (resultCode, data) -> {
                    DefaultBrowserPromoMetrics.recordOutcome(
                            mCurrentState,
                            DefaultBrowserPromoDeps.getInstance().getCurrentDefaultBrowserState());
                },
                null);
    }
}
