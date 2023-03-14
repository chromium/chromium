// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.signin.R;

/**
 * Implements AccountPickerBottomSheetStrings for the Send Tab To Self
 * EntryPoint to plumb the appropriate title, subtitle, and cancel button
 * strings to the {@link AccountPickerBottomSheetViewBinder}.
 */
public class AccountPickerBottomSheetSendTabToSelfStrings
        implements AccountPickerBottomSheetStrings {
    /** Returns the title string for the bottom sheet dialog. */
    @Override
    public @StringRes int getTitle() {
        return R.string.signin_account_picker_bottom_sheet_title_for_send_tab_to_self;
    }

    /** Returns the subtitle string for the bottom sheet dialog. */
    @Override
    public @StringRes int getSubtitle() {
        return R.string.signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self;
    }

    /** Returns the cancel button string for the bottom sheet dialog. */
    @Override
    public @StringRes int getCancelButton() {
        return R.string.cancel;
    }
}
