// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

/** The UI state required to properly display a 'update decorated' main menu button. */
public class MenuButtonState {
    /**
     * The new content description of the menu button.  Always set if this object is not
     * {@code null}.
     */
    public @StringRes int menuContentDescription;

    /**
     * An icon resource for the dark badge for the menu button.  Always set (not {@code 0}) if
     * this object is not {@code null}.
     */
    public @DrawableRes int darkBadgeIcon;

    /**
     * An icon resource for the light badge for the menu button.  Always set (not {@code 0}) if
     * this object is not {@code null}.
     */
    public @DrawableRes int lightBadgeIcon;

    /**
     * An icon resource for the badge for the menu button that adapts to light and dark modes.
     * Always set (not {@code 0}) if this object is not {@code null}.
     */
    public @DrawableRes int adaptiveBadgeIcon;
}
