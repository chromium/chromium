// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin.account_picker;
import androidx.annotation.StringRes;

/**
 * Interface to support different implementations for bottom sheet signin
 * dialog strings.
 */
public interface AccountPickerBottomSheetStrings {
    /** Returns the title string for the bottom sheet dialog. */
    public @StringRes int getTitle();
    /** Returns the subtitle string for the bottom sheet dialog. */
    public @StringRes int getSubtitle();
    /** Returns the cancel button string for the bottom sheet dialog. */
    public @StringRes int getCancelButton();
}
