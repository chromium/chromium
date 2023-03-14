// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin.account_picker;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.signin.R;
/**
 * Interface to support different implementations for bottom sheet signin
 * dialog strings.
 */
public interface AccountPickerBottomSheetStrings {
    /** Returns the title string for the bottom sheet dialog. */
    default @StringRes int getTitle() {
        return R.string.signin_account_picker_dialog_title;
    }
    /** Returns the subtitle string for the bottom sheet dialog. */
    default @StringRes int getSubtitle() {
        return R.string.signin_account_picker_bottom_sheet_subtitle;
    }
    /** Returns the cancel button string for the bottom sheet dialog. */
    default @StringRes int getCancelButton() {
        return R.string.signin_account_picker_dismiss_button;
    }
}
