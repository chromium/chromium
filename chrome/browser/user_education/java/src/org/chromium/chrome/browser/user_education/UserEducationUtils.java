// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Static utilities for User Education. */
@NullMarked
public class UserEducationUtils {
    private static final String OPTIONAL_PROMO_TYPE_SHOWN_HISTOGRAM =
            "Startup.Android.OptionalPromoTypeShown";

    // LINT.IfChange(OptionalPromoType)
    @IntDef({
        OptionalPromoType.UNKNOWN,
        OptionalPromoType.NONE_SHOWN,
        OptionalPromoType.PWA_RESTORE_PROMO,
        OptionalPromoType.FULLSCREEN_SIGNIN_PROMO,
        OptionalPromoType.DEFAULT_BROWSER_PROMO,
        OptionalPromoType.APP_LANGUAGE_PROMO,
        OptionalPromoType.APP_RATING_PROMPT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface OptionalPromoType {
        int UNKNOWN = 0;
        int NONE_SHOWN = 1;
        int PWA_RESTORE_PROMO = 2;
        int FULLSCREEN_SIGNIN_PROMO = 3;
        int DEFAULT_BROWSER_PROMO = 4;
        int APP_LANGUAGE_PROMO = 5;
        int APP_RATING_PROMPT = 6;
        int NUM_ENTRIES = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/startup/enums.xml:OptionalPromoType)

    /**
     * Records the type of optional promo shown.
     *
     * @param type The type of promo shown.
     */
    public static void recordOptionalPromoType(@OptionalPromoType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                OPTIONAL_PROMO_TYPE_SHOWN_HISTOGRAM, type, OptionalPromoType.NUM_ENTRIES);
    }
}
