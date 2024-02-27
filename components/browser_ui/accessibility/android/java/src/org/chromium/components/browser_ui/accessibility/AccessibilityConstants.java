// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

/** Constants used for accessibility classes. */
public final class AccessibilityConstants {
    /**
     * Page Zoom feature preferences. Tracks if a user wants the menu item always visible, and what
     * their default level of zoom should be.
     */
    public static final String PAGE_ZOOM_ALWAYS_SHOW_MENU_ITEM =
            "Chrome.PageZoom.AlwaysShowMenuItem.Refreshed";

    /** Tracks if a user wants Page Zoom to include an OS level adjustment for zoom level. */
    public static final String PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT =
            "Chrome.PageZoom.IncludeOSAdjustment";

    /** The preference keys for font size preferences. */
    public static final String FONT_USER_FONT_SCALE_FACTOR = "user_font_scale_factor";

    public static final String FONT_USER_SET_FORCE_ENABLE_ZOOM = "user_set_force_enable_zoom";
}
