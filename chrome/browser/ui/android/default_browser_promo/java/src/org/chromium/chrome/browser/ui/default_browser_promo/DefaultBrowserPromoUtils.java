// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A utility class providing information regarding states of default browser. */
public class DefaultBrowserPromoUtils {
    @IntDef({
        DefaultBrowserState.CHROME_DEFAULT,
        DefaultBrowserState.NO_DEFAULT,
        DefaultBrowserState.OTHER_DEFAULT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserState {
        int NO_DEFAULT = 0;
        int OTHER_DEFAULT = 1;

        /**
         * CHROME_DEFAULT means the currently running Chrome as opposed to
         * #isCurrentDefaultBrowserChrome() which looks for any Chrome.
         */
        int CHROME_DEFAULT = 2;

        int NUM_ENTRIES = 3;
    }

    private final DefaultBrowserPromoImpressionCounter mImpressionCounter;
    private final DefaultBrowserStateProvider mStateProvider;

    private static DefaultBrowserPromoUtils sInstance;

    DefaultBrowserPromoUtils(
            DefaultBrowserPromoImpressionCounter impressionCounter,
            DefaultBrowserStateProvider stateProvider) {
        mImpressionCounter = impressionCounter;
        mStateProvider = stateProvider;
    }

    public static DefaultBrowserPromoUtils getInstance() {
        if (sInstance == null) {
            sInstance =
                    new DefaultBrowserPromoUtils(
                            new DefaultBrowserPromoImpressionCounter(),
                            new DefaultBrowserStateProvider());
        }
        return sInstance;
    }

    static boolean isFeatureEnabled() {
        return !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO);
    }

    /**
     * Determine whether a promo dialog should be displayed or not. And prepare related logic to
     * launch promo if a promo dialog has been decided to display.
     *
     * @param activity The context.
     * @param windowAndroid The {@link WindowAndroid} for sending an intent.
     * @param track A {@link Tracker} for tracking role manager promo shown event.
     * @param ignoreMaxCount Whether the promo dialog should be shown irrespective of whether it has
     *     been shown before
     * @return True if promo dialog will be displayed.
     */
    public boolean prepareLaunchPromoIfNeeded(
            Activity activity,
            WindowAndroid windowAndroid,
            Tracker tracker,
            boolean ignoreMaxCount) {
        if (!shouldShowRoleManagerPromo(activity, ignoreMaxCount)) return false;
        mImpressionCounter.onPromoShown();
        tracker.notifyEvent("role_manager_default_browser_promos_shown");
        DefaultBrowserPromoManager manager =
                new DefaultBrowserPromoManager(
                        activity, windowAndroid, mImpressionCounter, mStateProvider);
        manager.promoByRoleManager();
        return true;
    }

    /**
     * Determine whether other types of default browser promo should be displayed if:
     * 1. Role Manager Promo shouldn't be shown,
     * 2. Impression count condition, other than the max count for RoleManager is met,
     * 3. Current default browser state satisfied the pre-defined conditions.
     */
    public boolean shouldShowOtherPromo(Context context) {
        return !shouldShowRoleManagerPromo(context, false)
                && mImpressionCounter.shouldShowPromo(true)
                && mStateProvider.shouldShowPromo();
    }

    /**
     * This decides whether the dialog should be promoted. Returns true if: the feature is enabled,
     * the {@link RoleManager} is available, and both the impression count and current default
     * browser state satisfied the pre-defined conditions.
     */
    public boolean shouldShowRoleManagerPromo(Context context, boolean ignoreMaxCount) {
        if (!isFeatureEnabled()) return false;

        if (!mStateProvider.isRoleAvailable(context)) {
            // Returns false if RoleManager default app setting is not available in the current
            // system.
            return false;
        }

        return mImpressionCounter.shouldShowPromo(ignoreMaxCount)
                && mStateProvider.shouldShowPromo();
    }

    /** Increment session count for triggering feature in the future. */
    public static void incrementSessionCount() {
        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT);
    }
}
