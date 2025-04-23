// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;

/** Interface that provides a theme overlay resource ID. */
@NullMarked
public interface ThemeOverlayProvider {
    /** Get the theme overlay resource ID provided by the instance. */
    @StyleRes
    int getThemeOverlay();
}
