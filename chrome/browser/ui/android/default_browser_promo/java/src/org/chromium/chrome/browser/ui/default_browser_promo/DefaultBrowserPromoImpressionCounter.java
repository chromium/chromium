// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.search_engines.SearchEngineChoiceService;

import java.util.concurrent.TimeUnit;

/**
 * A utility class providing the default browser promo impression counts, including total promo
 * count, session count, and intervals.
 */
@NullMarked
public class DefaultBrowserPromoImpressionCounter {
    private static final int MAX_PROMO_COUNT = 3;
    private static final int MIN_TRIGGER_SESSION_COUNT = 3;
    private static final int PROMO_SESSION_INTERVAL = 2;
    private static final int PROMO_TIME_INTERVAL_DAYS = 3;

    @VisibleForTesting static final String MAX_PROMO_COUNT_PARAM = "max_promo_count";

    DefaultBrowserPromoImpressionCounter() {}

    int getPromoCount() {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT, 0);
    }

    void onPromoShown() {
      incrementPromoCount();
      recordPromoTime();
      recordLastPromoSessionCount();
    }

    /**
     * This decides whether the promo should be promoted base on the impression count.
     * Return false if any of following criteria is met:
     *
     * <ol>
     *   <li>A promo dialog has been displayed before, unless {@code ignoreMaxCount} is true.
     *   <li>Not enough sessions have been started before.
     *   <li>Less than the promo interval if re-promoting.
     *   <li>Not enough time after a OS-level default browser selection was made.
     * </ol>
     *
     * @return boolean if promo dialog can be displayed.
     */
    boolean shouldShowPromo(boolean ignoreMaxCount) {
        return (ignoreMaxCount || getPromoCount() < getMaxPromoCount())
                && getSessionCount() >= getMinSessionCount()
                && getLastPromoInterval() >= getMinPromoInterval()
                && !SearchEngineChoiceService.getInstance().isDefaultBrowserPromoSuppressed();
    }

    private void incrementPromoCount() {
        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT);
    }

    int getMaxPromoCount() {
        return MAX_PROMO_COUNT;
    }

    int getSessionCount() {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT, 0);
    }

    int getLastPromoSessionCount() {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_SESSION_COUNT, 0);
    }

    private void recordLastPromoSessionCount() {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_SESSION_COUNT,
                        getSessionCount());
    }

    private void recordPromoTime() {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME,
                        (int) TimeUnit.MILLISECONDS.toMinutes(TimeUtils.currentTimeMillis()));
    }

    int getMinSessionCount() {
        if (getPromoCount() == 0) {
            return MIN_TRIGGER_SESSION_COUNT;
        } else {
            int sessionCountAtLastPromo = getLastPromoSessionCount();
            // If we've shown a promo before and the newer last promo session count hasn't
            // been set, assume the session count at the last time of promo was the minimum
            // required to show the promo.
            if (sessionCountAtLastPromo == 0) {
                sessionCountAtLastPromo = MIN_TRIGGER_SESSION_COUNT;
            }
            return PROMO_SESSION_INTERVAL + sessionCountAtLastPromo;
        }
    }

    int getLastPromoInterval() {
        int lastPromoTime =
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME, -1);
        if (lastPromoTime != -1) {
            return (int)
                    (TimeUnit.MILLISECONDS.toMinutes(TimeUtils.currentTimeMillis())
                            - lastPromoTime);
        }
        return Integer.MAX_VALUE;
    }

    int getMinPromoInterval() {
        // Scale the time delay by the number of times the promo has shown.
        // By default, expect 0 time delay for first promo show, 3 days for second promo
        // show and 6 days for third promo show.
        int timeIntervalMinutes =
                (int) (TimeUnit.DAYS.toMinutes(PROMO_TIME_INTERVAL_DAYS) * getPromoCount());
        return timeIntervalMinutes;
    }
}
