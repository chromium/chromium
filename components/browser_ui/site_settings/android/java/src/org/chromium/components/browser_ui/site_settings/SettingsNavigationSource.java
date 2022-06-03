// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Sources of navigation into settings screens, used for UMA tracking purposes.
 */
@Retention(RetentionPolicy.SOURCE)
@IntDef({
        SettingsNavigationSource.OTHER,
        SettingsNavigationSource.TWA_CLEAR_DATA_DIALOG,
        SettingsNavigationSource.TWA_MANAGE_SPACE_ACTIVITY,
})
public @interface SettingsNavigationSource {
    int OTHER = 0;
    int TWA_CLEAR_DATA_DIALOG = 1;
    int TWA_MANAGE_SPACE_ACTIVITY = 2;
    int NUM_ENTRIES = 3;

    // The key of the intent extra that is used for passing around the source.
    String EXTRA_KEY = "org.chromium.chrome.preferences.navigation_source";
}
