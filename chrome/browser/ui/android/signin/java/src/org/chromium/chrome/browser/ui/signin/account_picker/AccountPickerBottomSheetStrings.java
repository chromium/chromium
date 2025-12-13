// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin.account_picker;

import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Class containing strings for the sign-in account picker bottom sheet. */
@NullMarked
public final class AccountPickerBottomSheetStrings {
    public final String titleString;
    public final @Nullable String subtitleString;
    public final @Nullable String dismissButtonString;

    /**
     * Builder for {@link AccountPickerBottomSheetStrings} which contains strings for the sign-in
     * bottom sheet.
     */
    public static class Builder {
        private final String mTitleString;
        private @Nullable String mSubtitleString;
        private @Nullable String mDismissButtonString;

        /**
         * Creates the Builder for AccountPickerBottomSheetStrings.
         *
         * @param titleString The title string. Should not be empty.
         */
        public Builder(String titleString) {
            assert !TextUtils.isEmpty(titleString);
            mTitleString = titleString;
        }

        /**
         * Sets the subtitle string.
         *
         * @param subtitleString The subtitle string.
         */
        public Builder setSubtitleString(@Nullable String subtitleString) {
            mSubtitleString = subtitleString;
            return this;
        }

        /**
         * Sets the dismiss button string.
         *
         * @param dismissButtonString The dismiss button string.
         */
        public Builder setDismissButtonString(@Nullable String dismissButtonString) {
            mDismissButtonString = dismissButtonString;
            return this;
        }

        /** Builds the AccountPickerBottomSheetStrings. */
        public AccountPickerBottomSheetStrings build() {
            return new AccountPickerBottomSheetStrings(
                    mTitleString, mSubtitleString, mDismissButtonString);
        }
    }

    // Private constructor to enforce the use of the Builder.
    private AccountPickerBottomSheetStrings(
            String titleString,
            @Nullable String subtitleString,
            @Nullable String dismissButtonString) {
        this.titleString = titleString;
        this.subtitleString = subtitleString;
        this.dismissButtonString = dismissButtonString;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof AccountPickerBottomSheetStrings)) {
            return false;
        }
        AccountPickerBottomSheetStrings other = (AccountPickerBottomSheetStrings) object;
        return Objects.equals(titleString, other.titleString)
                && Objects.equals(subtitleString, other.subtitleString)
                && Objects.equals(dismissButtonString, other.dismissButtonString);
    }

    @Override
    public int hashCode() {
        return Objects.hash(titleString, subtitleString, dismissButtonString);
    }
}
