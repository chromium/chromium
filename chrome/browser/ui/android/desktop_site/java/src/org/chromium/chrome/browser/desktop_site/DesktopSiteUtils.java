// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_site;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.view.Display;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.SysUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/** Utility class for desktop sites. */
@NullMarked
public class DesktopSiteUtils {
    private static final String SITE_WILDCARD = "*";
    // Global defaults experiment constants.
    private static @Nullable DisplayMetrics sDisplayMetrics;
    @VisibleForTesting static @Nullable Boolean sDesktopUAAllowedOnExternalDisplayForOem;

    static final double DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES = 10.0;
    static final int DEFAULT_GLOBAL_SETTING_DEFAULT_ON_SMALLEST_SCREEN_WIDTH_THRESHOLD_DP = 600;
    static final int DEFAULT_GLOBAL_SETTING_DEFAULT_ON_MEMORY_LIMIT_THRESHOLD_MB = 6500;

    /**
     * Set or remove a domain level exception with URL for {@link
     * ContentSettingsType.REQUEST_DESKTOP_SITE}. Clear the subdomain level exception if any.
     *
     * @param profile Target profile whose content settings needs to be updated.
     * @param url {@link GURL} for the site that changes in desktop user agent.
     * @param useDesktopUserAgent True if the input |url| needs to use desktop user agent.
     */
    public static void setRequestDesktopSiteContentSettingsForUrl(
            Profile profile, GURL url, boolean useDesktopUserAgent) {
        boolean isOffTheRecord = profile.isOffTheRecord();
        String domainWildcardPattern =
                WebsitePreferenceBridge.toDomainWildcardPattern(url.getSpec());
        // Clear subdomain level exception if any.
        WebsitePreferenceBridge.setContentSettingCustomScope(
                profile,
                ContentSettingsType.REQUEST_DESKTOP_SITE,
                url.getHost(),
                /* secondaryPattern= */ SITE_WILDCARD,
                ContentSetting.DEFAULT);

        @ContentSetting
        int defaultValue =
                WebsitePreferenceBridge.getDefaultContentSetting(
                        profile, ContentSettingsType.REQUEST_DESKTOP_SITE);
        assert defaultValue == ContentSetting.ALLOW || defaultValue == ContentSetting.BLOCK;
        boolean rdsGlobalSetting = defaultValue == ContentSetting.ALLOW;
        @ContentSetting
        int contentSettingValue = useDesktopUserAgent ? ContentSetting.ALLOW : ContentSetting.BLOCK;
        // For normal profile, remove domain level setting if it matches the global setting.
        // For incognito profile, keep the domain level setting to override the settings from normal
        // profile.
        if (!isOffTheRecord && useDesktopUserAgent == rdsGlobalSetting) {
            // Keep the domain settings when the window setting preference is ON.
            PrefService prefService = UserPrefs.get(profile);
            if (!prefService.getBoolean(PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED)) {
                contentSettingValue = ContentSetting.DEFAULT;
            }
        }

        // Set or remove a domain level exception.
        WebsitePreferenceBridge.setContentSettingCustomScope(
                profile,
                ContentSettingsType.REQUEST_DESKTOP_SITE,
                domainWildcardPattern,
                /* secondaryPattern= */ SITE_WILDCARD,
                contentSettingValue);
    }

    /**
     * Determines whether the desktop site global setting should be enabled by default.
     *
     * @param displaySizeInInches The device primary display size, in inches.
     * @param context The current context.
     * @return Whether the desktop site global setting should be default-enabled.
     */
    static boolean shouldDefaultEnableGlobalSetting(double displaySizeInInches, Context context) {
        // Desktop Android always requests desktop sites.
        if (DeviceInfo.isDesktop()) {
            return true;
        }

        // Do not default-enable if memory is below threshold.
        if (SysUtils.amountOfPhysicalMemoryKB()
                < DEFAULT_GLOBAL_SETTING_DEFAULT_ON_MEMORY_LIMIT_THRESHOLD_MB
                        * ConversionUtils.KILOBYTES_PER_MEGABYTE) {
            return false;
        }

        // Do not default-enable for x86 devices.
        if (!isCpuArchitectureArm()) {
            return false;
        }

        // Do not default-enable on external display.
        if (isOnExternalDisplay(context)) {
            return false;
        }

        // Do not default-enable if the screen size in inches is below threshold.
        if (displaySizeInInches < DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES) {
            return false;
        }

        // Do not default-enable if the smallest screen size in dp is below threshold.
        if (context.getResources().getConfiguration().smallestScreenWidthDp
                < DEFAULT_GLOBAL_SETTING_DEFAULT_ON_SMALLEST_SCREEN_WIDTH_THRESHOLD_DP) {
            return false;
        }

        // Do not default-enable if the setting has already been default enabled; or the user has
        // explicitly updated the setting.
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        boolean previouslyDefaultEnabled =
                sharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, false);
        boolean previouslyUpdatedByUser =
                sharedPreferencesManager.contains(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY);
        return !previouslyDefaultEnabled && !previouslyUpdatedByUser;
    }

    /**
     * Default-enables the desktop site global setting if {@code shouldDefaultEnableGlobalSetting}
     * returns true.
     *
     * @param displaySizeInInches The device primary display size, in inches.
     * @param profile The current {@link Profile}.
     * @param context The current context.
     * @return Whether the desktop site global setting was default-enabled.
     */
    public static boolean maybeDefaultEnableGlobalSetting(
            double displaySizeInInches, Profile profile, Context context) {
        if (!shouldDefaultEnableGlobalSetting(displaySizeInInches, context)) {
            return false;
        }

        WebsitePreferenceBridge.setCategoryEnabled(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, true);
        return true;
    }

    /**
     * Default-enables the desktop site window setting if Chrome is opened on a tablet-sized
     * internal display.
     *
     * @param activity The current {@link Activity}.
     * @param profile The current {@link Profile}.
     */
    public static void maybeDefaultEnableWindowSetting(Activity activity, Profile profile) {
        int smallestScreenWidthDp = DisplayUtil.getCurrentSmallestScreenWidth(activity);
        boolean isOnExternalDisplay = isOnExternalDisplay(activity);
        if (isOnExternalDisplay
                || smallestScreenWidthDp < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP
                || DeviceInfo.isDesktop()) {
            return;
        }
        PrefService prefService = UserPrefs.get(profile);
        if (prefService.isDefaultValuePreference(PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED)) {
            prefService.setBoolean(
                    PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED, /* value= */ true);
        }
    }

    /**
     * Determines whether the desktop site should be overridden for the current URL.
     *
     * @param profile The current {@link Profile}.
     * @param url The current URL.
     * @param context The current context.
     * @return Whether the desktop site should be overridden for the current URL.
     */
    public static boolean shouldOverrideDesktopSite(
            Profile profile, @Nullable GURL url, Context context) {
        // For --request-desktop-sites, always override the user agent.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.REQUEST_DESKTOP_SITES)) {
            return true;
        }

        // If domain/global setting is enabled and window setting should not
        // apply, override the user agent.
        if (readRequestDesktopSiteContentSettings(profile, url)
                && !shouldApplyWindowSetting(profile, url, context)) {
            return true;
        }

        // Enable on large connected displays only when user has not explicitly set preference.
        if (isOnEligibleExternalDisplayForDesktopUA(context)
                && !hasUserUpdatedContentSettings(url, profile)) {
            return true;
        }
        return false;
    }

    /**
     * Check if Request Desktop Site global setting is enabled.
     *
     * @param profile The profile of the tab. Content settings have separate storage for incognito
     *     profiles. For site-specific exceptions the actual profile is needed.
     * @param url The URL for the current web content.
     * @return Whether the desktop site should be requested.
     */
    public static boolean isDesktopSiteEnabled(Profile profile, GURL url) {
        return WebsitePreferenceBridge.getContentSetting(
                        profile, ContentSettingsType.REQUEST_DESKTOP_SITE, url, url)
                == ContentSetting.ALLOW;
    }

    private static boolean hasUserUpdatedContentSettings(@Nullable GURL url, Profile profile) {
        // Global setting would not apply when user overrides via domain settings.
        if (!isRequestDesktopSiteContentSettingsGlobal(profile, url)) return true;

        // Using global settings.
        // Check if user has updated global setting preference.
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        return sharedPreferencesManager.contains(
                PrefNames.REQUEST_DESKTOP_SITE_GLOBAL_SETTING_USER_ENABLED);
    }

    /**
     * Determine whether RDS window setting should be applied. When returning 'true' the mobile user
     * agent should be used for the current window size.
     */
    static boolean shouldApplyWindowSetting(Profile profile, @Nullable GURL url, Context context) {
        // Skip window setting on Automotive and revisit if / when they add split screen.
        if (DeviceInfo.isAutomotive()) {
            return false;
        }
        PrefService prefService = UserPrefs.get(profile);
        if (!prefService.getBoolean(PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED)) {
            return false;
        }
        if (!isRequestDesktopSiteContentSettingsGlobal(profile, url)) {
            return false;
        }
        // Try the window attributes width first.
        // PCCT has its width stored in window attributes.
        int widthPixels = -1;
        Activity activity = ContextUtils.activityFromContext(context);
        // activity might be null in integration tests.
        if (activity != null && activity.getWindow() != null) {
            widthPixels = activity.getWindow().getAttributes().width;
        }
        DisplayMetrics displayMetrics = getDisplayMetricsFromContext(context);
        // Use width from displayMetrics if the window attributes width is invalid.
        if (widthPixels <= 0) {
            widthPixels = displayMetrics.widthPixels;
        }
        return widthPixels / displayMetrics.density < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
    }

    /**
     * Retrieve the {@link DisplayMetrics} from {@link Context} for the current window.
     *
     * @param context The current {@link Context}.
     * @return The {@link DisplayMetrics} for the current window.
     */
    private static DisplayMetrics getDisplayMetricsFromContext(Context context) {
        if (sDisplayMetrics != null) {
            return sDisplayMetrics;
        }
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        display.getMetrics(displayMetrics);
        return displayMetrics;
    }

    /** Check if the CPU architecture is ARM. */
    private static boolean isCpuArchitectureArm() {
        String[] abiStrings = Build.SUPPORTED_ABIS;
        if (abiStrings == null || abiStrings.length == 0) {
            return false;
        }
        return abiStrings[0].toLowerCase(Locale.ROOT).contains("arm");
    }

    static boolean isOnExternalDisplay(Context context) {
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        return display.getDisplayId() != Display.DEFAULT_DISPLAY;
    }

    private static boolean isOnEligibleExternalDisplayForDesktopUA(Context context) {
        // Do not enable on default display.
        if (!isOnExternalDisplay(context)) {
            return false;
        }

        // Do not enable on displays smaller than threshold.
        DisplayAndroid currentDisplay = DisplayAndroid.getNonMultiDisplay(context);
        double displaySizeInInches = DisplayUtil.getDisplaySizeInInches(currentDisplay);
        if (displaySizeInInches < DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES) {
            return false;
        }

        // Do not enable on OEMs not allowlisted.
        if (!ChromeFeatureList.sDesktopUAOnConnectedDisplay.isEnabled()) {
            return false;
        }
        if (sDesktopUAAllowedOnExternalDisplayForOem == null) {
            Set<String> allowlist = new HashSet<>();
            String allowlistStr =
                    ChromeFeatureList.sDesktopUAAllowedOnExternalDisplayForOem.getValue();
            if (!TextUtils.isEmpty(allowlistStr)) {
                Collections.addAll(allowlist, allowlistStr.split(","));
            }
            sDesktopUAAllowedOnExternalDisplayForOem =
                    allowlist.isEmpty()
                            || allowlist.contains(Build.MANUFACTURER.toLowerCase(Locale.US));
        }
        return sDesktopUAAllowedOnExternalDisplayForOem;
    }

    /** Check if Request Desktop Site ContentSettings is global setting. */
    private static boolean isRequestDesktopSiteContentSettingsGlobal(
            Profile profile, @Nullable GURL url) {
        if (url == null) {
            return true;
        }
        return WebsitePreferenceBridge.isContentSettingGlobal(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE, url, url);
    }

    /** Read Request Desktop Site ContentSettings. */
    private static boolean readRequestDesktopSiteContentSettings(
            Profile profile, @Nullable GURL url) {
        return url != null && isDesktopSiteEnabled(profile, url);
    }

    @VisibleForTesting
    static void setTestDisplayMetrics(DisplayMetrics displayMetrics) {
        sDisplayMetrics = displayMetrics;
    }

    @VisibleForTesting
    public static void setDefaultEnableGlobalSettingForTesting(
            boolean defaultEnableGlobalSetting) {}
}
