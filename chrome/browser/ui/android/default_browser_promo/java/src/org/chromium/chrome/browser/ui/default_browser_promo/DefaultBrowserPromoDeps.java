// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoManager.P_NO_DEFAULT_PROMO_STRATEGY;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoAction;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;

import java.util.concurrent.TimeUnit;

/**
 * A utility class providing information regarding external states of the system to facilitate
 * testing and interacting with external states by {@link SharedPreferencesManager},
 * {@link PackageManagerUtils} and {@link RoleManager}.
 */
public class DefaultBrowserPromoDeps {
    private static final int MAX_PROMO_COUNT = 1;
    private static final int MIN_TRIGGER_SESSION_COUNT = 3;
    private static final String SESSION_COUNT_PARAM = "min_trigger_session_count";
    private static final String PROMO_COUNT_PARAM = "max_promo_count";
    private static final String PROMO_INTERVAL_PARAM = "promo_interval";

    static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";

    // TODO(crbug.com/1090103): move to some util class for reuse.
    static final String[] CHROME_PACKAGE_NAMES = {CHROME_STABLE_PACKAGE_NAME, "org.chromium.chrome",
            "com.chrome.canary", "com.chrome.beta", "com.chrome.dev"};
    private static DefaultBrowserPromoDeps sInstance;

    private DefaultBrowserPromoDeps() {}

    public static DefaultBrowserPromoDeps getInstance() {
        if (sInstance == null) sInstance = new DefaultBrowserPromoDeps();
        return sInstance;
    }

    boolean isFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO)
                && !CommandLine.getInstance().hasSwitch(
                        ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO);
    }

    int getPromoCount() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT, 0);
    }

    void incrementPromoCount() {
        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT);
    }

    int getMaxPromoCount() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, PROMO_COUNT_PARAM,
                MAX_PROMO_COUNT);
    }

    int getSessionCount() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT, 0);
    }

    void recordPromoTime() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME,
                (int) TimeUnit.MILLISECONDS.toMinutes(System.currentTimeMillis()));
    }

    int getMinSessionCount() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, SESSION_COUNT_PARAM,
                MIN_TRIGGER_SESSION_COUNT);
    }

    int getLastPromoInterval() {
        int lastPromoTime = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME, -1);
        if (lastPromoTime != -1) {
            return (int) (TimeUnit.MILLISECONDS.toMinutes(System.currentTimeMillis())
                    - lastPromoTime);
        }
        return Integer.MAX_VALUE;
    }

    int getMinPromoInterval() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, PROMO_INTERVAL_PARAM, 0);
    }

    boolean isCurrentDefaultBrowserChrome(ResolveInfo info) {
        String packageName = info.activityInfo.packageName;
        for (String name : CHROME_PACKAGE_NAMES) {
            if (name.equals(packageName)) return true;
        }
        return false;
    }

    @DefaultBrowserState
    public int getCurrentDefaultBrowserState() {
        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        return getCurrentDefaultBrowserState(info);
    }

    @DefaultBrowserState
    int getCurrentDefaultBrowserState(ResolveInfo info) {
        if (info == null || info.match == 0) return DefaultBrowserState.NO_DEFAULT; // no default
        if (TextUtils.equals(ContextUtils.getApplicationContext().getPackageName(),
                    info.activityInfo.packageName)) {
            return DefaultBrowserState.CHROME_DEFAULT; // Already default
        }
        return DefaultBrowserState.OTHER_DEFAULT;
    }

    boolean doesManageDefaultAppsSettingsActivityExist() {
        if (getSDKInt() < Build.VERSION_CODES.N) return false;
        ResolveInfo info = PackageManagerUtils.resolveActivity(
                new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS), 0);
        return info != null && info.match != 0;
    }

    boolean isChromeStable() {
        return ContextUtils.getApplicationContext().getPackageName().equals(
                CHROME_STABLE_PACKAGE_NAME);
    }

    boolean isChromePreStableInstalled() {
        for (ResolveInfo info : PackageManagerUtils.queryAllWebBrowsersInfo()) {
            for (String name : CHROME_PACKAGE_NAMES) {
                if (name.equals(CHROME_STABLE_PACKAGE_NAME)) continue;
                if (name.equals(info.activityInfo.packageName)) return true;
            }
        }
        return false;
    }

    int getSDKInt() {
        return Build.VERSION.SDK_INT;
    }

    int promoActionOnP() {
        String promoOnP = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, P_NO_DEFAULT_PROMO_STRATEGY);
        if (TextUtils.equals(promoOnP, "disabled")) {
            return DefaultBrowserPromoAction.NO_ACTION;
        } else if (TextUtils.equals(promoOnP, "system_settings")) {
            return DefaultBrowserPromoAction.SYSTEM_SETTINGS;
        } else {
            return DefaultBrowserPromoAction.DISAMBIGUATION_SHEET;
        }
    }

    @SuppressLint("NewApi")
    boolean isRoleAvailable(Activity activity) {
        if (getSDKInt() < Build.VERSION_CODES.Q) {
            return false;
        }
        RoleManager roleManager = (RoleManager) activity.getSystemService(Context.ROLE_SERVICE);
        boolean isRoleAvailable = roleManager.isRoleAvailable(RoleManager.ROLE_BROWSER);
        boolean isRoleHeld = roleManager.isRoleHeld(RoleManager.ROLE_BROWSER);
        return isRoleAvailable && !isRoleHeld;
    }
}
