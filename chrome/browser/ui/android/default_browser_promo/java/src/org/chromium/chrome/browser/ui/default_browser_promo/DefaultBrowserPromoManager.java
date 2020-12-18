// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoDialog.DialogStyle;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoMetrics.UIDismissalReason;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manage all types of default browser promo dialogs and listen to the activity state change to
 * trigger dialogs.
 */
public class DefaultBrowserPromoManager {
    private static final String SKIP_PRIMER_PARAM = "skip_primer";
    static final String P_NO_DEFAULT_PROMO_STRATEGY = "p_no_default_promo";

    private final Activity mActivity;
    private DefaultBrowserPromoDialog mDialog;
    private @DefaultBrowserState int mCurrentState;
    private WindowAndroid mWindowAndroid;

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
        boolean shouldSkipPrimer = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, SKIP_PRIMER_PARAM, false);
        Runnable onOK = () -> {
            RoleManager roleManager =
                    (RoleManager) mActivity.getSystemService(Context.ROLE_SERVICE);

            DefaultBrowserPromoMetrics.recordRoleManagerShow(mCurrentState);
            if (!shouldSkipPrimer) {
                DefaultBrowserPromoMetrics.recordUiDismissalReason(
                        mCurrentState, UIDismissalReason.CHANGE_DEFAULT);
            }

            Intent intent = roleManager.createRequestRoleIntent(RoleManager.ROLE_BROWSER);
            mWindowAndroid.showCancelableIntent(intent, (window, resultCode, data) -> {
                DefaultBrowserPromoMetrics.recordOutcome(mCurrentState,
                        DefaultBrowserPromoDeps.getInstance().getCurrentDefaultBrowserState());
            }, null);
        };
        if (shouldSkipPrimer) {
            onOK.run();
        } else {
            showDialog(DefaultBrowserPromoDialog.DialogStyle.ROLE_MANAGER, onOK);
        }
    }

    private void showDialog(@DialogStyle int style, Runnable okCallback) {
        mDialog = DefaultBrowserPromoDialog.createDialog(mActivity, style, okCallback, () -> {
            DefaultBrowserPromoMetrics.recordUiDismissalReason(
                    mCurrentState, UIDismissalReason.NO_THANKS);
        });

        DefaultBrowserPromoMetrics.recordDialogShow(mCurrentState);
        mDialog.show();
    }

    @VisibleForTesting
    DefaultBrowserPromoDialog getDialogForTesting() {
        return mDialog;
    }
}
