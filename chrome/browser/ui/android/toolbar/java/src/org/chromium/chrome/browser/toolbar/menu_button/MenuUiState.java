// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import androidx.annotation.Nullable;

/**
 * The UI state required to properly decorate the main menu.  This may include the button
 * decorations as well as the actual update item to show in the menu.
 */
public class MenuUiState {
    /**
     * The optional UI state for building the menu item.  If {@code null} no item should be
     * shown.
     */
    public @Nullable MenuItemState itemState;

    /**
     * The optional UI state for decorating the menu button itself.  If {@code null} no
     * decoration should be applied to the menu button.
     */
    public @Nullable MenuButtonState buttonState;
}
