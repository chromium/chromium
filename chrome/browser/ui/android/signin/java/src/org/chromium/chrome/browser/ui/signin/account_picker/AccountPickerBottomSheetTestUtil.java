// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.signin.metrics.SigninAccessPoint;

final class AccountPickerBottomSheetTestUtil {
    static AccountPickerBottomSheetStrings getBottomSheetStrings(
            @SigninAccessPoint int accessPoint) {
        switch (accessPoint) {
            case SigninAccessPoint.SEND_TAB_TO_SELF_PROMO:
                return new AccountPickerBottomSheetStrings(
                        R.string.signin_account_picker_bottom_sheet_title_for_send_tab_to_self,
                        R.string.signin_account_picker_bottom_sheet_subtitle_for_send_tab_to_self,
                        R.string.cancel);
            case SigninAccessPoint.WEB_SIGNIN:
                return new AccountPickerBottomSheetStrings(
                        R.string.signin_account_picker_dialog_title,
                        R.string.signin_account_picker_bottom_sheet_subtitle,
                        R.string.signin_account_picker_dismiss_button);
            case SigninAccessPoint.BOOKMARK_MANAGER:
                return new AccountPickerBottomSheetStrings(R.string.sign_in_to_chrome, 0, 0);
            default:
                throw new IllegalArgumentException("Access point strings not handled in tests.");
        }
    }
}
