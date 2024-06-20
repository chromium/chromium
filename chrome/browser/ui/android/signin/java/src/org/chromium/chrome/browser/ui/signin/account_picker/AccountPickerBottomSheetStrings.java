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

    // Private constructor to enforce the use of the Builder.
    private AccountPickerBottomSheetStrings(
            @StringRes int titleStringId,
            @StringRes int subtitleStringId,
            @StringRes int dismissButtonStringId) {
        this.titleStringId = titleStringId;
        this.subtitleStringId = subtitleStringId;
        this.dismissButtonStringId = dismissButtonStringId;
    }

    /**
     * Builder for {@link AccountPickerBottomSheetStrings} which contains string IDs for the sign-in
     * bottom sheet.
     */
    public static class Builder {
        private final @StringRes int mTitleStringId;
        private @StringRes int mSubtitleStringId;
        private @StringRes int mDismissButtonStringId;

        /**
         * Creates the Builder for AccountPickerBottomSheetStrings.
         *
         * @param titleStringId ID for the title string. Should be a non-zero valid string ID.
         */
        public Builder(@StringRes int titleStringId) {
            assert titleStringId != 0;
            mTitleStringId = titleStringId;
        }

        /**
         * Sets the resource ID for the bottom sheet subtitle string.
         *
         * @param stringId ID for the subtitle string.
         */
        public Builder setSubtitleStringId(@StringRes int stringId) {
            mSubtitleStringId = stringId;
            return this;
        }

        /**
         * Sets the resource ID for the dismiss button string.
         *
         * @param stringId ID for the dismiss button string.
         */
        public Builder setDismissButtonStringId(@StringRes int stringId) {
            mDismissButtonStringId = stringId;
            return this;
        }

        /** Builds the AccountPickerBottomSheetStrings. */
        public AccountPickerBottomSheetStrings build() {
            return new AccountPickerBottomSheetStrings(
                    mTitleStringId, mSubtitleStringId, mDismissButtonStringId);
        }
    }
}
