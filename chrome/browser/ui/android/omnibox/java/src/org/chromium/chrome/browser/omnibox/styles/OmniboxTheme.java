// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({OmniboxTheme.LIGHT_THEME, OmniboxTheme.DARK_THEME, OmniboxTheme.INCOGNITO,
        OmniboxTheme.DEFAULT})
@Retention(RetentionPolicy.SOURCE)
public @interface OmniboxTheme {
    /* Light branded color as defined by the website, unrelated to the app/OS dark theme setting. */
    int LIGHT_THEME = 0;
    /* Dark branded color as defined by the website, unrelated to the app/OS dark theme setting. */
    int DARK_THEME = 1;
    /* Incognito theme. */
    int INCOGNITO = 2;
    /* Default theme with potentially dynamic colors that can be light or dark. */
    int DEFAULT = 3;
}
