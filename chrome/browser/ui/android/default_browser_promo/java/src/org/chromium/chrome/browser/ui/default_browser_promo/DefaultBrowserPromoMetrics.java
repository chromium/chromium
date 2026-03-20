// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper class to record histograms. */
@NullMarked
public class DefaultBrowserPromoMetrics {
    /**
     * The source/location of the default browser promo that was clicked.
     *
     * <p>Note: this should be kept in sync with DefaultBrowserPromoSourceType in
     * tools/metrics/histograms/metadata/android/enums.xml.
     */
    // LINT.IfChange(DefaultBrowserPromoSourceType)
    @IntDef({
        DefaultBrowserPromoSourceType.MESSAGES_PROMO,
        DefaultBrowserPromoSourceType.SETTING_CARD_PROMO,
        DefaultBrowserPromoSourceType.EDUCATIONAL_TIP_PROMO
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserPromoSourceType {
        int MESSAGES_PROMO = 0;
        int SETTING_CARD_PROMO = 1;
        int EDUCATIONAL_TIP_PROMO = 2;

        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:DefaultBrowserPromoSourceType)

    private static String getSourceSuffix(@DefaultBrowserPromoEntryPoint int source) {
        if (source == DefaultBrowserPromoEntryPoint.APP_MENU) {
            return "AppMenu";
        } else if (source == DefaultBrowserPromoEntryPoint.SETTINGS) {
            return "Settings";
        }
        return "";
    }

    /**
     * Record the click event on an entry point.
     *
     * @param source The source of the click ("AppMenu" or "Settings").
     * @param currentState The state of the browser (OTHER_DEFAULT, CHROME_DEFAULT, etc.) at the
     *     moment of the click.
     */
    static void recordEntrypointClick(
            @DefaultBrowserPromoEntryPoint int source, @DefaultBrowserState int currentState) {
        String suffix = getSourceSuffix(source);
        if (suffix.isEmpty()) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Android.DefaultBrowserPromo.EntryPoint." + suffix,
                currentState,
                DefaultBrowserState.NUM_ENTRIES);
    }

    /**
     * Record {@link DefaultBrowserState} when role manager dialog is shown.
     *
     * @param currentState The {@link DefaultBrowserState} when the dialog is shown.
     */
    static void recordRoleManagerShow(@DefaultBrowserState int currentState) {
        assert currentState != DefaultBrowserState.CHROME_DEFAULT;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DefaultBrowserPromo.RoleManagerShown",
                currentState,
                DefaultBrowserState.NUM_ENTRIES);
    }

    /**
     * Record the outcome of the default browser promo for a specific source.
     *
     * @param newState The {@link DefaultBrowserState} after the user changes the default.
     * @param source The source of the promo (e.g. "AppMenu").
     */
    static void recordOutcome(
            @DefaultBrowserState int newState, @DefaultBrowserPromoEntryPoint int source) {
        String suffix = getSourceSuffix(source);
        if (suffix.isEmpty()) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Android.DefaultBrowserPromo.Outcome." + suffix,
                newState,
                DefaultBrowserState.NUM_ENTRIES);
    }

    /**
     * Record the outcome of the default browser promo.
     *
     * @param oldState The {@link DefaultBrowserState} when the dialog shown.
     * @param newState The {@link DefaultBrowserState} after user changes default.
     * @param promoCount The number of times the promo has shown.
     */
    static void recordOutcome(
            @DefaultBrowserState int oldState, @DefaultBrowserState int newState, int promoCount) {
        assert oldState != DefaultBrowserState.CHROME_DEFAULT;
        String name =
                oldState == DefaultBrowserState.NO_DEFAULT
                        ? "Android.DefaultBrowserPromo.Outcome.NoDefault"
                        : "Android.DefaultBrowserPromo.Outcome.OtherDefault";
        RecordHistogram.recordEnumeratedHistogram(name, newState, DefaultBrowserState.NUM_ENTRIES);

        String postFix;
        switch (promoCount) {
            case 1:
                postFix = ".FirstPromo";
                break;
            case 2:
                postFix = ".SecondPromo";
                break;
            case 3:
                postFix = ".ThirdPromo";
                break;
            case 4:
                postFix = ".FourthPromo";
                break;
            default:
                postFix = ".FifthOrMorePromo";
        }
        name += postFix;
        RecordHistogram.recordEnumeratedHistogram(name, newState, DefaultBrowserState.NUM_ENTRIES);
    }

    /**
     * Record the click event on a default browser promo.
     *
     * @param promoType The source/type of promo that was clicked.
     */
    public static void recordPromoClick(@DefaultBrowserPromoSourceType int promoType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DefaultBrowserPromo.Click",
                promoType,
                DefaultBrowserPromoSourceType.NUM_ENTRIES);
    }
}
