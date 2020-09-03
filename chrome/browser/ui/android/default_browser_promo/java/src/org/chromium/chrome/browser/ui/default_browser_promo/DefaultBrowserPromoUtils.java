// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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

    @IntDef({DefaultBrowserPromoAction.SYSTEM_SETTINGS,
            DefaultBrowserPromoAction.DISAMBIGUATION_SHEET, DefaultBrowserPromoAction.ROLE_MANAGER,
            DefaultBrowserPromoAction.NO_ACTION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserPromoAction {
        int SYSTEM_SETTINGS = 0;
        int DISAMBIGUATION_SHEET = 1;
        int ROLE_MANAGER = 2;
        int NO_ACTION = 3;
    }

    private static final String DISAMBIGUATION_SHEET_PROMOED_KEY_PREFIX =
            "disambiguation_sheet_promoed.";
    static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";

    // TODO(crbug.com/1090103): move to some util class for reuse.
    static final String[] CHROME_PACKAGE_NAMES = {CHROME_STABLE_PACKAGE_NAME, "org.chromium.chrome",
            "com.chrome.canary", "com.chrome.beta", "com.chrome.dev"};

    /**
     * Determine whether a promo dialog should be displayed or not. And prepare related logic to
     * launch promo if a promo dialog has been decided to display.
     *
     * @param activity The context.
     * @param dispatcher The {@link ActivityLifecycleDispatcher} of the current activity.
     * @param windowAndroid The {@link WindowAndroid} for sending an intent.
     * @return True if promo dialog will be displayed.
     */
    public static boolean prepareLaunchPromoIfNeeded(Activity activity,
            ActivityLifecycleDispatcher dispatcher, WindowAndroid windowAndroid) {
        DefaultBrowserPromoDeps deps = DefaultBrowserPromoDeps.getInstance();
        int action = decideNextAction(deps, activity);
        if (action == DefaultBrowserPromoAction.NO_ACTION) return false;
        deps.incrementPromoCount();
        deps.recordPromoTime();
        DefaultBrowserPromoManager manager = new DefaultBrowserPromoManager(
                activity, dispatcher, windowAndroid, deps.getCurrentDefaultBrowserState());
        if (action == DefaultBrowserPromoAction.ROLE_MANAGER) {
            manager.promoByRoleManager();
        } else if (action == DefaultBrowserPromoAction.SYSTEM_SETTINGS) {
            manager.promoBySystemSettings();
        } else if (action == DefaultBrowserPromoAction.DISAMBIGUATION_SHEET) {
            manager.promoByDisambiguationSheet();
        }
        return true;
    }

    /**
     * This decides in which way and style the dialog should be promoed.
     * Returns No Action if any of following criteria is met:
     *      1. A promo dialog has been displayed before.
     *      2. Not enough sessions have been started before.
     *      3. Any chrome, including pre-stable, has been set as default.
     *      4. On Chrome stable while no default browser is set and multiple chrome channels
     *         are installed.
     *      5. Less than the promo interval if re-promoing.
     *      6. A browser other than chrome channel is default and default app setting is not
     *         available in the current system.
     */
    @DefaultBrowserPromoAction
    static int decideNextAction(DefaultBrowserPromoDeps deps, Activity activity) {
        if (!deps.isFeatureEnabled()) {
            return DefaultBrowserPromoAction.NO_ACTION;
        }
        // Criteria 1
        if (deps.getPromoCount() >= deps.getMaxPromoCount()) {
            return DefaultBrowserPromoAction.NO_ACTION;
        }
        // Criteria 2
        if (deps.getSessionCount() < deps.getMinSessionCount()) {
            return DefaultBrowserPromoAction.NO_ACTION;
        }
        // Criteria 5
        if (deps.getLastPromoInterval() < deps.getMinPromoInterval()) {
            return DefaultBrowserPromoAction.NO_ACTION;
        }

        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        int state = deps.getCurrentDefaultBrowserState(info);
        int action = DefaultBrowserPromoAction.NO_ACTION;
        if (state == DefaultBrowserState.CHROME_DEFAULT) {
            action = DefaultBrowserPromoAction.NO_ACTION;
        } else if (state == DefaultBrowserState.NO_DEFAULT) {
            // Criteria 4
            if (deps.isChromeStable() && deps.isChromePreStableInstalled()) {
                action = DefaultBrowserPromoAction.NO_ACTION;
            } else if (deps.getSDKInt() >= Build.VERSION_CODES.Q) {
                action = DefaultBrowserPromoAction.ROLE_MANAGER;
            } else {
                action = deps.promoActionOnP();
            }
        } else { // other default
            // Criteria 3
            if (deps.isCurrentDefaultBrowserChrome(info)) {
                action = DefaultBrowserPromoAction.NO_ACTION;
            } else {
                action = deps.getSDKInt() >= Build.VERSION_CODES.Q
                        ? DefaultBrowserPromoAction.ROLE_MANAGER
                        : DefaultBrowserPromoAction.SYSTEM_SETTINGS;
            }
        }
        // Criteria 6
        if (action == DefaultBrowserPromoAction.SYSTEM_SETTINGS
                && !deps.doesManageDefaultAppsSettingsActivityExist()) {
            action = DefaultBrowserPromoAction.NO_ACTION;
        } else if (action == DefaultBrowserPromoAction.ROLE_MANAGER
                && !deps.isRoleAvailable(activity)) {
            action = DefaultBrowserPromoAction.NO_ACTION;
        }
        return action;
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
        DefaultBrowserPromoMetrics.recordOutcome(previousState,
                DefaultBrowserPromoDeps.getInstance().getCurrentDefaultBrowserState());
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
                    DefaultBrowserPromoDeps.getInstance().getCurrentDefaultBrowserState());
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
}
