// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.base.ContextUtils;
import org.chromium.content_public.browser.ContentFeatureList;

/**
 * General purpose utils class for page zoom feature. This is for methods that are shared by both
 * the settings UI and the MVC component (e.g. shared prefs calls), and is accessed by each
 * individually rather than having the settings UI depend on the MVC component.
 */
public class PageZoomUtils {
    // The default value for zoom that user can change in the accessibility settings page.
    public static final int PAGE_ZOOM_DEFAULT_ZOOM_VALUE = 50;

    /**
     * Returns whether the Accessibility Settings page should include the 'Zoom' UI. The page
     * should always display the UI if the feature is enabled.
     * @return boolean
     */
    public static boolean shouldShowSettingsUI() {
        return ContentFeatureList.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM);
    }

    // Methods to interact with SharedPreferences. These do not use SharedPreferencesManager so
    // that they can be used in //components.

    /**
     * Returns the current user choice for default zoom level (set in Accessibility Settings).
     * @return int
     */
    public static int getDefaultZoomValue() {
        return ContextUtils.getAppSharedPreferences().getInt(
                AccessibilityConstants.PAGE_ZOOM_DEFAULT_ZOOM_SETTING,
                PAGE_ZOOM_DEFAULT_ZOOM_VALUE);
    }

    /**
     * Set a new user choice for default zoom level.
     * @param newValue int
     */
    public static void setDefaultZoomValue(int newValue) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(AccessibilityConstants.PAGE_ZOOM_DEFAULT_ZOOM_SETTING, newValue)
                .apply();
    }

    /**
     * Returns the current user choice for always showing the Zoom AppMenu item (set in
     * Accessibility Settings).
     * @return boolean
     */
    public static boolean getShouldAlwaysShowZoomValue() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                AccessibilityConstants.PAGE_ZOOM_ALWAYS_SHOW_MENU_ITEM, false);
    }

    /**
     * Set a new user choice for always showing the Zoom AppMenu item.
     * @param newValue boolean
     */
    public static void setShouldAlwaysShowZoomValue(boolean newValue) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AccessibilityConstants.PAGE_ZOOM_ALWAYS_SHOW_MENU_ITEM, newValue)
                .apply();
    }
}