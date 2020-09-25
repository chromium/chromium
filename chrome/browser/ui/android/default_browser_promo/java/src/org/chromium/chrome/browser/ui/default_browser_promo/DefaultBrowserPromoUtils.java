// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoManager.P_NO_DEFAULT_PROMO_STRATEGY;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * A utility class providing information regarding states of default browser.
 */
public class DefaultBrowserPromoUtils {
    @IntDef({DefaultBrowserState.CHROME_DEFAULT, DefaultBrowserState.NO_DEFAULT,
            DefaultBrowserState.OTHER_DEFAULT})
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

    private static final int MIN_TRIGGER_SESSION_COUNT = 3;
    private static final int MAX_PROMO_COUNT = 1;
    private static final String SESSION_COUNT_PARAM = "min_trigger_session_count";
    private static final String PROMO_COUNT_PARAM = "max_promo_count";
    private static final String PROMO_INTERVAL_PARAM = "promo_interval";

    private static final String DISAMBIGUATION_SHEET_PROMOED_KEY_PREFIX =
            "disambiguation_sheet_promoed.";
    static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";

    // TODO(crbug.com/1090103): move to some util class for reuse.
    static final String[] CHROME_PACKAGE_NAMES = {CHROME_STABLE_PACKAGE_NAME, "org.chromium.chrome",
            "com.chrome.canary", "com.chrome.beta", "com.chrome.dev"};

    /**
     * Determine whether a promo dialog should be displayed or not. And prepare related logic to
     * launch promo if a promo dialog has been decided to display.
     * Return false if any of following criteria is met:
     *      1. A promo dialog has been displayed before.
     *      2. Not enough sessions have been started before.
     *      3. Any chrome, including pre-stable, has been set as default.
     *      4. On Chrome stable while no default browser is set and multiple chrome channels
     *         are installed.
     *      5. Less than the promo interval if re-promoing.
     *      6. A browser other than chrome channel is default and default app setting is not
     *         available in the current system.
     *
     * @param activity The context.
     * @param dispatcher The {@link ActivityLifecycleDispatcher} of the current activity.
     * @param windowAndroid The {@link WindowAndroid} for sending an intent.
     * @return True if promo dialog will be displayed.
     */
    public static boolean prepareLaunchPromoIfNeeded(Activity activity,
            ActivityLifecycleDispatcher dispatcher, WindowAndroid windowAndroid) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO)) {
            return false;
        }

        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO)) {
            return false;
        }

        // Criteria 1
        int maxPromoCount = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, PROMO_COUNT_PARAM,
                MAX_PROMO_COUNT);

        if (SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT, 0)
                >= maxPromoCount) {
            return false;
        }

        // Criteria 2
        int minSessionCount = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, SESSION_COUNT_PARAM,
                MIN_TRIGGER_SESSION_COUNT);

        if (SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT, 0)
                < minSessionCount) {
            return false;
        }

        // Criteria 5
        int lastPromoTime = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME, -1);
        if (lastPromoTime != -1) {
            int promoInterval = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, PROMO_INTERVAL_PARAM, 0);
            if (TimeUnit.MILLISECONDS.toMinutes(System.currentTimeMillis()) - lastPromoTime
                    < promoInterval) {
                return false;
            }
        }

        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        int state = getCurrentDefaultBrowserState(info);

        // Already default
        if (state == DefaultBrowserState.CHROME_DEFAULT) return false;

        // Criteria 3 & Criteria 6
        if (state == DefaultBrowserState.OTHER_DEFAULT
                && (isCurrentDefaultBrowserChrome(info)
                        || !doesManageDefaultAppsSettingsActivityExist())) {
            // Default apps setting activity does not exist on L and M.  Early return
            // before we write prefs and record metrics to skip the call to promo.
            return false;
        }

        // Criteria 4
        if (ContextUtils.getApplicationContext().getPackageName().equals(CHROME_STABLE_PACKAGE_NAME)
                && isChromePreStableInstalled()
                && state == DefaultBrowserState.NO_DEFAULT) {
            return false;
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q
                && state == DefaultBrowserState.NO_DEFAULT) {
            String promoOnP = ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, P_NO_DEFAULT_PROMO_STRATEGY);
            if (TextUtils.equals(promoOnP, "disabled")) {
                return false;
            } else if (TextUtils.equals(promoOnP, "system_settings")
                    && !doesManageDefaultAppsSettingsActivityExist()) {
                return false;
            }
        }

        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME,
                (int) TimeUnit.MILLISECONDS.toMinutes(System.currentTimeMillis()));
        DefaultBrowserPromoManager.create(activity, dispatcher, windowAndroid).promo(state);
        return true;
    }

    /**
     * Increment session count for triggering feature in the future.
     */
    public static void incrementSessionCount() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO)) return;
        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT);
    }

    /**
     * The current {@link DefaultBrowserState} in the system.
     */
    @DefaultBrowserState
    public static int getCurrentDefaultBrowserState() {
        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        return getCurrentDefaultBrowserState(info);
    }

    /**
     * Check the result of default browser promo on start up if the default browser promo dialog is
     * displayed in this session or last session and the result has not been recorded yet.
     */
    public static void maybeRecordOutcomeOnStart() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO)) return;
        if (!SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_BY_SYSTEM_SETTINGS, false)) {
            return;
        }
        int previousState = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_DEFAULT_STATE);
        DefaultBrowserPromoMetrics.recordOutcome(previousState, getCurrentDefaultBrowserState());
        // reset
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_BY_SYSTEM_SETTINGS, false);
    }

    /**
     * Called on new intent is received on the activity so that we can record some metrics.
     */
    public static void onNewIntentReceived(Intent intent) {
        boolean promoed = intent.getBooleanExtra(getDisambiguationSheetPromoedKey(), false);
        if (promoed) {
            DefaultBrowserPromoMetrics.recordLaunchedByDisambiguationSheet(
                    getCurrentDefaultBrowserState());
        }
    }

    static String getDisambiguationSheetPromoedKey() {
        return DISAMBIGUATION_SHEET_PROMOED_KEY_PREFIX
                + ContextUtils.getApplicationContext().getPackageName();
    }

    /**
     * Remove intent data if this intent is triggered by default browser promo; Otherwise,
     * chrome will open a new tab.
     */
    public static void maybeRemoveIntentData(Intent intent) {
        if (intent.getBooleanExtra(getDisambiguationSheetPromoedKey(), false)) {
            // Intent with Uri.EMPTY as data will be ignored by the IntentHandler.
            intent.setData(Uri.EMPTY);
        }
    }

    @VisibleForTesting
    static boolean isChromePreStableInstalled() {
        for (ResolveInfo info : PackageManagerUtils.queryAllWebBrowsersInfo()) {
            for (String name : CHROME_PACKAGE_NAMES) {
                if (name.equals(CHROME_STABLE_PACKAGE_NAME)) continue;
                if (name.equals(info.activityInfo.packageName)) return true;
            }
        }
        return false;
    }

    @VisibleForTesting
    static boolean isCurrentDefaultBrowserChrome(ResolveInfo info) {
        String packageName = info.activityInfo.packageName;
        for (String name : CHROME_PACKAGE_NAMES) {
            if (name.equals(packageName)) return true;
        }
        return false;
    }

    @VisibleForTesting
    @DefaultBrowserState
    static int getCurrentDefaultBrowserState(ResolveInfo info) {
        if (info == null || info.match == 0) return DefaultBrowserState.NO_DEFAULT; // no default
        if (TextUtils.equals(ContextUtils.getApplicationContext().getPackageName(),
                    info.activityInfo.packageName)) {
            return DefaultBrowserState.CHROME_DEFAULT; // Already default
        }
        return DefaultBrowserState.OTHER_DEFAULT;
    }

    private static boolean doesManageDefaultAppsSettingsActivityExist() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return false;
        ResolveInfo info = PackageManagerUtils.resolveActivity(
                new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS), 0);
        return info != null && info.match != 0;
    }
}
