// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.SharedPreferences;

/** Constants used for {@link SingleCategorySettings}. */
public class SingleCategorySettingsConstants {
    /**
     * {@link SharedPreferences} key that indicates whether the desktop site global setting was
     * enabled by the user.
     */
    public static final String USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY =
            "Chrome.RequestDesktopSiteGlobalSetting.UserEnabled";
}
