// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.StringRes;

/* Class containing string resource ids for the sign-in account picker bottom sheet. */
public final class AccountPickerBottomSheetStrings {
    public final @StringRes int titleStringId;
    public final @StringRes int subtitleStringId;
    public final @StringRes int dismissButtonStringId;

    /**
     * Create the object containing string ids for the sign-in bottom sheet.
     *
     * @param titleStringId ID for the title string. Should be a non-zero valid string ID.
     * @param subtitleStringId ID for the subtitle string.
     * @param dismissButtonStringId ID for the skip button string.
     */
    public AccountPickerBottomSheetStrings(
            @StringRes int titleStringId,
            @StringRes int subtitleStringId,
            @StringRes int dismissButtonStringId) {
        assert titleStringId != 0;

        this.titleStringId = titleStringId;
        this.subtitleStringId = subtitleStringId;
        this.dismissButtonStringId = dismissButtonStringId;
    }
}
