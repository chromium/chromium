// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({OmniboxTheme.LIGHT_THEME, OmniboxTheme.DARK_THEME, OmniboxTheme.INCOGNITO})
@Retention(RetentionPolicy.SOURCE)
public @interface OmniboxTheme {
    int LIGHT_THEME = 0;
    int DARK_THEME = 1;
    int INCOGNITO = 2;
}
