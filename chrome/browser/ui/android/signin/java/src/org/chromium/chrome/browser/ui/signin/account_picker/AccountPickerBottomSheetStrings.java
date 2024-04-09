// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin.account_picker;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.StringRes;

/* Class containing string resource ids for the sign-in account picker bottom sheet. */
public final class AccountPickerBottomSheetStrings implements Parcelable {
    public final @StringRes int titleStringId;
    public final @StringRes int subtitleStringId;
    public final @StringRes int dismissButtonStringId;

    public static final Parcelable.Creator<AccountPickerBottomSheetStrings> CREATOR =
            new Parcelable.Creator<AccountPickerBottomSheetStrings>() {
                @Override
                public AccountPickerBottomSheetStrings createFromParcel(Parcel in) {
                    return new AccountPickerBottomSheetStrings(in);
                }

                @Override
                public AccountPickerBottomSheetStrings[] newArray(int size) {
                    return new AccountPickerBottomSheetStrings[size];
                }
            };

    // Private constructor to enforce the use of the Builder.
    private AccountPickerBottomSheetStrings(
            @StringRes int titleStringId,
            @StringRes int subtitleStringId,
            @StringRes int dismissButtonStringId) {
        this.titleStringId = titleStringId;
        this.subtitleStringId = subtitleStringId;
        this.dismissButtonStringId = dismissButtonStringId;
    }

    private AccountPickerBottomSheetStrings(Parcel in) {
        titleStringId = in.readInt();
        subtitleStringId = in.readInt();
        dismissButtonStringId = in.readInt();
    }

    /** Implements {@link Parcelable} */
    @Override
    public int describeContents() {
        return 0;
    }

    /** Implements {@link Parcelable} */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(titleStringId);
        out.writeInt(subtitleStringId);
        out.writeInt(dismissButtonStringId);
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
