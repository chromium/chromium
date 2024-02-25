// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.annotation.SuppressLint;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;

import java.util.concurrent.TimeUnit;

/**
 * A utility class providing information regarding external states of the system to facilitate
 * testing and interacting with external states by {@link ChromeSharedPreferences},
 * {@link PackageManagerUtils} and {@link RoleManager}.
 */
public class DefaultBrowserPromoDeps {
    private static final int MAX_PROMO_COUNT = 1;
    private static final int MIN_TRIGGER_SESSION_COUNT = 3;
    private static final int MIN_PROMO_INTERVAL = 0;

    @VisibleForTesting static final String MAX_PROMO_COUNT_PARAM = "max_promo_count";

    @VisibleForTesting
    static final String PROMO_TIME_INTERVAL_DAYS_PARAM = "promo_time_interval_days";

    @VisibleForTesting static final String PROMO_SESSION_INTERVAL_PARAM = "promo_session_interval";

    static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";

    // TODO(crbug.com/1090103): move to some util class for reuse.
    static final String[] CHROME_PACKAGE_NAMES = {
        CHROME_STABLE_PACKAGE_NAME,
        "org.chromium.chrome",
        "com.chrome.canary",
        "com.chrome.beta",
        "com.chrome.dev"
    };
    private static DefaultBrowserPromoDeps sInstance;

    @VisibleForTesting
    DefaultBrowserPromoDeps() {}

    public static DefaultBrowserPromoDeps getInstance() {
        if (sInstance == null) sInstance = new DefaultBrowserPromoDeps();
        return sInstance;
    }

    boolean isFeatureEnabled() {
        return !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO);
    }

    int getPromoCount() {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT, 0);
    }

    void incrementPromoCount() {
        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT);
    }

    int getMaxPromoCount() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID, MAX_PROMO_COUNT_PARAM, 3);
        }
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

    void recordLastPromoSessionCount() {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_SESSION_COUNT,
                        getSessionCount());
    }

    void recordPromoTime() {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME,
                        (int) TimeUnit.MILLISECONDS.toMinutes(System.currentTimeMillis()));
    }

    int getMinSessionCount() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)) {
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
                int promoSessionInterval =
                        ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                                ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID,
                                PROMO_SESSION_INTERVAL_PARAM,
                                2);

                return promoSessionInterval + sessionCountAtLastPromo;
            }
        }
        return MIN_TRIGGER_SESSION_COUNT;
    }

    int getLastPromoInterval() {
        int lastPromoTime =
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME, -1);
        if (lastPromoTime != -1) {
            return (int)
                    (TimeUnit.MILLISECONDS.toMinutes(System.currentTimeMillis()) - lastPromoTime);
        }
        return Integer.MAX_VALUE;
    }

    int getMinPromoInterval() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)) {
            int timeIntervalDays =
                    ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                            ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID,
                            PROMO_TIME_INTERVAL_DAYS_PARAM,
                            3);
            // Scale the time delay by the number of times the promo has shown.
            // By default, expect 0 time delay for first promo show, 3 days for second promo
            // show and 6 days for third promo show.
            int timeIntervalMinutes =
                    (int) (TimeUnit.DAYS.toMinutes(timeIntervalDays) * getPromoCount());
            return timeIntervalMinutes;
        }
        return MIN_PROMO_INTERVAL;
    }

    boolean isCurrentDefaultBrowserChrome(ResolveInfo info) {
        String packageName = info.activityInfo.packageName;
        for (String name : CHROME_PACKAGE_NAMES) {
            if (name.equals(packageName)) return true;
        }
        return false;
    }

    public @DefaultBrowserState int getCurrentDefaultBrowserState() {
        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        return getCurrentDefaultBrowserState(info);
    }

    @DefaultBrowserState
    int getCurrentDefaultBrowserState(ResolveInfo info) {
        if (info == null || info.match == 0) return DefaultBrowserState.NO_DEFAULT; // no default
        if (TextUtils.equals(
                ContextUtils.getApplicationContext().getPackageName(),
                info.activityInfo.packageName)) {
            return DefaultBrowserState.CHROME_DEFAULT; // Already default
        }
        return DefaultBrowserState.OTHER_DEFAULT;
    }

    boolean doesManageDefaultAppsSettingsActivityExist() {
        ResolveInfo info =
                PackageManagerUtils.resolveActivity(
                        new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS), 0);
        return info != null && info.match != 0;
    }

    boolean isChromeStable() {
        return ContextUtils.getApplicationContext()
                .getPackageName()
                .equals(CHROME_STABLE_PACKAGE_NAME);
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

    ResolveInfo getDefaultWebBrowserActivityResolveInfo() {
        return PackageManagerUtils.resolveDefaultWebBrowserActivity();
    }

    int getSDKInt() {
        return Build.VERSION.SDK_INT;
    }

    @SuppressLint("NewApi")
    boolean isRoleAvailable(Context context) {
        if (getSDKInt() < Build.VERSION_CODES.Q) {
            return false;
        }
        RoleManager roleManager = (RoleManager) context.getSystemService(Context.ROLE_SERVICE);
        if (roleManager == null) return false;
        boolean isRoleAvailable = roleManager.isRoleAvailable(RoleManager.ROLE_BROWSER);
        boolean isRoleHeld = roleManager.isRoleHeld(RoleManager.ROLE_BROWSER);
        return isRoleAvailable && !isRoleHeld;
    }

    static void setInstanceForTesting(DefaultBrowserPromoDeps instance) {
        sInstance = instance;
    }
}
