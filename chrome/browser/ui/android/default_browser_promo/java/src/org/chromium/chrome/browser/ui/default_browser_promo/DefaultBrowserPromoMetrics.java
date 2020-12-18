// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to record histograms related to the default browser promo.
 */
class DefaultBrowserPromoMetrics {
    @IntDef({UIDismissalReason.CHANGE_DEFAULT, UIDismissalReason.NO_THANKS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UIDismissalReason {
        int CHANGE_DEFAULT = 0;
        int NO_THANKS = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * Record the reason why the promo dialog is dismissed.
     * @param currentState The {@link DefaultBrowserState} when the dialog is shown.
     * @param reason The {@link UIDismissalReason} indicating the dismissal reason.
     */
    static void recordUiDismissalReason(
            @DefaultBrowserState int currentState, @UIDismissalReason int reason) {
        assert currentState != DefaultBrowserState.CHROME_DEFAULT;
        if (currentState == DefaultBrowserState.NO_DEFAULT) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DefaultBrowserPromo.UIDismissalReason.NoDefault", reason,
                    UIDismissalReason.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DefaultBrowserPromo.UIDismissalReason.OtherDefault", reason,
                    UIDismissalReason.NUM_ENTRIES);
        }
    }

    /**
     * Record {@link DefaultBrowserState} when the dialog shown.
     * @param currentState The {@link DefaultBrowserState} when the dialog is shown.
     */
    static void recordDialogShow(@DefaultBrowserState int currentState) {
        assert currentState != DefaultBrowserState.CHROME_DEFAULT;
        RecordHistogram.recordEnumeratedHistogram("Android.DefaultBrowserPromo.DialogShown",
                currentState, DefaultBrowserState.NUM_ENTRIES);
    }

    /**
     * Record {@link DefaultBrowserState} when role manager dialog is shown.
     * @param currentState The {@link DefaultBrowserState} when the dialog is shown.
     */
    static void recordRoleManagerShow(@DefaultBrowserState int currentState) {
        assert currentState != DefaultBrowserState.CHROME_DEFAULT;
        RecordHistogram.recordEnumeratedHistogram("Android.DefaultBrowserPromo.RoleManagerShown",
                currentState, DefaultBrowserState.NUM_ENTRIES);
    }

    /**
     * Record the outcome of the default browser promo.
     * @param oldState The {@link DefaultBrowserState} when the dialog shown.
     * @param newState The {@link DefaultBrowserState} after user changes default.
     */
    static void recordOutcome(
            @DefaultBrowserState int oldState, @DefaultBrowserState int newState) {
        assert oldState != DefaultBrowserState.CHROME_DEFAULT;
        String name = oldState == DefaultBrowserState.NO_DEFAULT
                ? "Android.DefaultBrowserPromo.Outcome.NoDefault"
                : "Android.DefaultBrowserPromo.Outcome.OtherDefault";
        RecordHistogram.recordEnumeratedHistogram(name, newState, DefaultBrowserState.NUM_ENTRIES);
    }
}
