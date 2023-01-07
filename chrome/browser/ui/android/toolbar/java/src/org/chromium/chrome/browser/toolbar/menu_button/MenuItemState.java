// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

/** The UI state required to properly display an update-related main menu item. */
public class MenuItemState {
    /** The title resource of the menu.  Always set if this object is not {@code null}. */
    public @StringRes int title;

    /** The color resource of the title.  Always set if this object is not {@code null}. */
    public @ColorRes int titleColorId;

    /** The summary string for the menu.  Maybe {@code null} if no summary should be shown. */
    public @Nullable String summary;

    /** An icon resource for the menu item.  May be {@code 0} if no icon is specified. */
    public @DrawableRes int icon;

    /** The color resource of the icon tint.  May be {@code 0} if no tint is specified. */
    public @ColorRes int iconTintId;

    /** Whether or not the menu item should be enabled (and clickable). */
    public boolean enabled;
}
