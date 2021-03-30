// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Theme setting variations. This is also used for histograms and should therefore be treated
 * as append-only. See DarkThemePreferences in tools/metrics/histograms/enums.xml.
 */
@IntDef({ThemeType.SYSTEM_DEFAULT, ThemeType.LIGHT, ThemeType.DARK})
@Retention(RetentionPolicy.SOURCE)
public @interface ThemeType {
    // Values are used for indexing tables - should start from 0 and can't have gaps.
    int SYSTEM_DEFAULT = 0;
    int LIGHT = 1;
    int DARK = 2;

    int NUM_ENTRIES = 3;
}
