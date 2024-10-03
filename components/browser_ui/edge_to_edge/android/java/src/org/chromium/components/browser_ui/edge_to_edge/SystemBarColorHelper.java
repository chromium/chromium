// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import androidx.annotation.ColorInt;

import org.chromium.base.lifetime.Destroyable;

/** Interface for setting system bar color. */
public interface SystemBarColorHelper extends Destroyable {

    /** Set the status bar color. */
    void setStatusBarColor(@ColorInt int color);

    /** Set the navigation bar color. */
    void setNavigationBarColor(@ColorInt int color);

    /** Set the navigation bar color. */
    void setNavigationBarDividerColor(@ColorInt int dividerColor);
}
