// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.theme;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
    BrandedColorScheme.LIGHT_BRANDED_THEME,
    BrandedColorScheme.DARK_BRANDED_THEME,
    BrandedColorScheme.INCOGNITO,
    BrandedColorScheme.APP_DEFAULT
})
@Retention(RetentionPolicy.SOURCE)
public @interface BrandedColorScheme {
    /**
     * Light branded color as defined by the website, unrelated to the app/OS dark theme setting.
     */
    int LIGHT_BRANDED_THEME = 0;

    /** Dark branded color as defined by the website, unrelated to the app/OS dark theme setting. */
    int DARK_BRANDED_THEME = 1;

    /** Incognito theme. */
    int INCOGNITO = 2;

    /**
     * Default theme with potentially dynamic colors that can be light or dark depending on user
     * or system settings.
     */
    int APP_DEFAULT = 3;
}
