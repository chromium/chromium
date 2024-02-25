// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;

import org.chromium.components.browser_ui.site_settings.AutoDarkMetrics;
import org.chromium.components.browser_ui.site_settings.AutoDarkMetrics.AutoDarkSettingsChangeSource;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/**
 * A controller class could enable or disable web content dark mode feature based on the content
 * settings {@link ContentSettingsType.AUTO_DARK_WEB_CONTENT}.
 */
public class WebContentsDarkModeController {
    /**
     * Return whether auto dark mode is enable for a given URL.
     * @param browserContextHandle Current browser context handle.
     * @param url Queried URL to check whether auto dark is enabled.
     * @return Whether auto dark mode is enable for a given URL.
     */
    public static boolean isEnabledForUrl(BrowserContextHandle browserContextHandle, GURL url) {
        @ContentSettingValues
        int contentSetting =
                WebsitePreferenceBridge.getContentSetting(
                        browserContextHandle, ContentSettingsType.AUTO_DARK_WEB_CONTENT, url, url);
        return contentSetting != ContentSettingValues.BLOCK;
    }

    /**
     * Set whether auto dark mode is enable for a given URL.
     * @param browserContextHandle Current browser context handle.
     * @param url Queried URL whether auto dark is enabled.
     * @param enabled Whether auto dark should enabled for the url.
     */
    public static void setEnabledForUrl(
            BrowserContextHandle browserContextHandle, GURL url, boolean enabled) {
        // This is only called when a user disables/enables the feature for a site from the app
        // menu. The app menu item should only be visible (and thus clickable) if Auto Dark is
        // enabled. If it is enabled, the default content setting should be ALLOW.
        assert WebsitePreferenceBridge.getDefaultContentSetting(
                        browserContextHandle, ContentSettingsType.AUTO_DARK_WEB_CONTENT)
                == ContentSettingValues.ALLOW;

        @ContentSettingValues
        int contentSettingValue =
                enabled ? ContentSettingValues.DEFAULT : ContentSettingValues.BLOCK;

        WebsitePreferenceBridge.setContentSettingDefaultScope(
                browserContextHandle,
                ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                url,
                url,
                contentSettingValue);
        AutoDarkMetrics.recordAutoDarkSettingsChangeSource(
                AutoDarkSettingsChangeSource.APP_MENU, enabled);
    }

    /**
     * Enable or disable the global user settings for auto dark mode. If the global settings is
     * enabled, the web contents will be darkened by default if Chrome is in dark mode.
     * @param browserContextHandle Current browser context handle.
     * @param enabled The new global setting state of the web content auto dark mode.
     */
    public static void setGlobalUserSettings(
            BrowserContextHandle browserContextHandle, boolean enabled) {
        // This function is only used by Theme Settings so far. If this function has additional
        // call sites, change the AutoDarkSettingsChangeSource as well.
        WebsitePreferenceBridge.setContentSettingEnabled(
                browserContextHandle, ContentSettingsType.AUTO_DARK_WEB_CONTENT, enabled);
        AutoDarkMetrics.recordAutoDarkSettingsChangeSource(
                AutoDarkSettingsChangeSource.THEME_SETTINGS, enabled);
    }

    /**
     * Return whether web content dark mode is enabled by settings, despite whether the current
     * activity is in night mode.
     * @param browserContextHandle Current browser context handle.
     * */
    public static boolean isGlobalUserSettingsEnabled(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridge.isContentSettingEnabled(
                browserContextHandle, ContentSettingsType.AUTO_DARK_WEB_CONTENT);
    }

    /**
     * Whether web contents dark mode feature is enabled for the UI.
     * Returns true when auto dark global setting is enabled, and context is in night mode.
     * @param context {@link Context} used to check whether UI is in night mode.
     * @param browserContextHandle Current browser context handle.
     * */
    public static boolean isFeatureEnabled(
            Context context, BrowserContextHandle browserContextHandle) {
        return WebContentsDarkModeController.isGlobalUserSettingsEnabled(browserContextHandle)
                && ColorUtils.inNightMode(context);
    }

    /**
     * Records UKM when the user disables auto-dark theming for a site through the app menu.
     * @param webContents The web contents associated with the current tab.
     * @param enabled The new per-site setting state for the current site.
     */
    public static void recordAutoDarkUkm(WebContents webContents, boolean enabled) {
        if (enabled) return;
        new UkmRecorder.Bridge()
                .recordEventWithBooleanMetric(
                        webContents, "Android.DarkTheme.AutoDarkMode", "DisabledByUser");
    }

    /**
     * Return the current enabled state for auto dark mode. If the input {@link GURL} is not null,
     * the enabled state will also check if auto dark is enabled for URL.
     * @param browserContextHandle Current browser context handle.
     * @param context {@link Context} used to check whether UI is in night mode.
     * @param url Queried URL whether auto dark is enabled.
     * @return Whether auto dark is enabled for the given input.
     */
    public static boolean getEnabledState(
            BrowserContextHandle browserContextHandle, Context context, GURL url) {
        if (!isGlobalUserSettingsEnabled(browserContextHandle)) {
            return false;
        }
        if (!ColorUtils.inNightMode(context)) {
            return false;
        }
        if (!url.isEmpty() && !isEnabledForUrl(browserContextHandle, url)) {
            return false;
        }
        return true;
    }
}
