// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.base.CoreAccountId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Class containing configurations for the bottom sheet based sign-in view and the history sync
 * opt-in view.
 */
public final class BottomSheetSigninAndHistorySyncConfig implements Parcelable {

    /** The sign-in step that should be shown to the user when there's no account on the device. */
    @IntDef({
        NoAccountSigninMode.BOTTOM_SHEET,
        NoAccountSigninMode.ADD_ACCOUNT,
        NoAccountSigninMode.NO_SIGNIN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NoAccountSigninMode {
        /** Show the 0-account version of the sign-in bottom sheet. */
        int BOTTOM_SHEET = 0;

        /** Bring the user to GMS Core to add an account, then sign-in with the new account. */
        int ADD_ACCOUNT = 1;

        /** No sign-in should be done, the entry point should not be visible to the user. */
        int NO_SIGNIN = 2;
    }

    /** The sign-in step that should be shown to the user when there's 1+ accounts on the device. */
    @IntDef({
        WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
        WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WithAccountSigninMode {
        /** Show the "collapsed" sign-in bottom sheet containing the default account. */
        int DEFAULT_ACCOUNT_BOTTOM_SHEET = 0;

        /** Show the "expanded" sign-in bottom sheet containing the accounts list. */
        int CHOOSE_ACCOUNT_BOTTOM_SHEET = 1;
    }

    public final @NonNull AccountPickerBottomSheetStrings bottomSheetStrings;
    public final @NoAccountSigninMode int noAccountSigninMode;
    public final @WithAccountSigninMode int withAccountSigninMode;
    public final @HistorySyncConfig.OptInMode int historyOptInMode;
    public final @Nullable CoreAccountId selectedCoreAccountId;

    /** Builder for {@link BottomSheetSigninAndHistorySyncConfig}. */
    public static class Builder {
        private @NonNull AccountPickerBottomSheetStrings mBottomSheetStrings;
        private @NoAccountSigninMode int mNoAccountSigninMode;
        private @WithAccountSigninMode int mWithAccountSigninMode;
        private @HistorySyncConfig.OptInMode int mHistoryOptInMode;
        private @Nullable CoreAccountId mSelectedCoreAccountId;

        /**
         * Constructor of the Builder.
         *
         * @param bottomSheetStrings the strings shown in the sign-in bottom sheet.
         * @param noAccountSigninMode The type of UI that should be shown for the sign-in step if
         *     there's no account on the device.
         * @param withAccountSigninMode The type of UI that should be shown for the sign-in step if
         *     there are 1+ accounts on the device.
         * @param historyOptInMode Whether the history opt-in should be always, optionally or never
         *     shown.
         */
        public Builder(
                @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
                @NoAccountSigninMode int noAccountSigninMode,
                @WithAccountSigninMode int withAccountSigninMode,
                @HistorySyncConfig.OptInMode int historyOptInMode) {
            mBottomSheetStrings = bottomSheetStrings;
            mNoAccountSigninMode = noAccountSigninMode;
            mWithAccountSigninMode = withAccountSigninMode;
            mHistoryOptInMode = historyOptInMode;
        }

        /**
         * @param selectedCoreAccountId The account that should be displayed in the sign-in bottom
         *     sheet. If null, the default account will be displayed.
         */
        public Builder selectedCoreAccountId(CoreAccountId selectedCoreAccountId) {
            mSelectedCoreAccountId = selectedCoreAccountId;
            return this;
        }

        public BottomSheetSigninAndHistorySyncConfig build() {
            return new BottomSheetSigninAndHistorySyncConfig(
                    mBottomSheetStrings,
                    mNoAccountSigninMode,
                    mWithAccountSigninMode,
                    mHistoryOptInMode,
                    mSelectedCoreAccountId);
        }
    }

    private BottomSheetSigninAndHistorySyncConfig(
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @Nullable CoreAccountId selectedCoreAccountId) {
        assert bottomSheetStrings != null;

        this.bottomSheetStrings = bottomSheetStrings;
        this.noAccountSigninMode = noAccountSigninMode;
        this.withAccountSigninMode = withAccountSigninMode;
        this.historyOptInMode = historyOptInMode;
        this.selectedCoreAccountId = selectedCoreAccountId;
    }

    private BottomSheetSigninAndHistorySyncConfig(Parcel in) {
        this(
                in.readParcelable(AccountPickerBottomSheetStrings.class.getClassLoader()),
                /* noAccountSigninMode= */ in.readInt(),
                /* withAccountSigninMode= */ in.readInt(),
                /* historyOptInMode= */ in.readInt(),
                /* selectedCoreAccountId= */ getCoreAccountId(in.readString()));
    }

    private static @Nullable CoreAccountId getCoreAccountId(@Nullable String id) {
        return id == null ? null : new CoreAccountId(id);
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof BottomSheetSigninAndHistorySyncConfig)) {
            return false;
        }

        BottomSheetSigninAndHistorySyncConfig other =
                (BottomSheetSigninAndHistorySyncConfig) object;
        return bottomSheetStrings.equals(other.bottomSheetStrings)
                && noAccountSigninMode == other.noAccountSigninMode
                && withAccountSigninMode == other.withAccountSigninMode
                && historyOptInMode == other.historyOptInMode
                && Objects.equals(selectedCoreAccountId, other.selectedCoreAccountId);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                bottomSheetStrings,
                noAccountSigninMode,
                withAccountSigninMode,
                historyOptInMode,
                selectedCoreAccountId);
    }

    /** Implements {@link Parcelable} */
    @Override
    public int describeContents() {
        return 0;
    }

    /** Implements {@link Parcelable} */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeParcelable(bottomSheetStrings, 0);
        out.writeInt(noAccountSigninMode);
        out.writeInt(withAccountSigninMode);
        out.writeInt(historyOptInMode);
        String id = selectedCoreAccountId == null ? null : selectedCoreAccountId.getId();
        out.writeString(id);
    }

    public static final Parcelable.Creator<BottomSheetSigninAndHistorySyncConfig> CREATOR =
            new Parcelable.Creator<BottomSheetSigninAndHistorySyncConfig>() {
                @Override
                public BottomSheetSigninAndHistorySyncConfig createFromParcel(Parcel in) {
                    return new BottomSheetSigninAndHistorySyncConfig(in);
                }

                @Override
                public BottomSheetSigninAndHistorySyncConfig[] newArray(int size) {
                    return new BottomSheetSigninAndHistorySyncConfig[size];
                }
            };
}
