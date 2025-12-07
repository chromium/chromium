// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Class containing configurations for the bottom sheet based sign-in view and the history sync
 * opt-in view.
 */
@NullMarked
public final class BottomSheetSigninAndHistorySyncConfig {

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
        WithAccountSigninMode.SEAMLESS_SIGNIN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WithAccountSigninMode {
        /** Show the "collapsed" sign-in bottom sheet containing the default account. */
        int DEFAULT_ACCOUNT_BOTTOM_SHEET = 0;

        /** Show the "expanded" sign-in bottom sheet containing the accounts list. */
        int CHOOSE_ACCOUNT_BOTTOM_SHEET = 1;

        /** Sign-in immediately without displaying the intermediate sign-in bottom sheet. */
        int SEAMLESS_SIGNIN = 2;
    }

    public final AccountPickerBottomSheetStrings bottomSheetStrings;
    public final HistorySyncConfig historySyncConfig;
    public final @NoAccountSigninMode int noAccountSigninMode;
    public final @WithAccountSigninMode int withAccountSigninMode;
    public final @HistorySyncConfig.OptInMode int historyOptInMode;
    public final @Nullable CoreAccountId selectedCoreAccountId;
    public final boolean shouldShowSigninSnackbar;

    /** Builder for {@link BottomSheetSigninAndHistorySyncConfig}. */
    public static class Builder {
        private final AccountPickerBottomSheetStrings mBottomSheetStrings;
        private final String mHistorySyncTitle;
        private final String mHistorySyncSubtitle;
        private final @NoAccountSigninMode int mNoAccountSigninMode;
        private final @HistorySyncConfig.OptInMode int mHistoryOptInMode;
        private @WithAccountSigninMode int mWithAccountSigninMode;
        private @Nullable CoreAccountId mSelectedCoreAccountId;
        private boolean mShouldShowSigninSnackbar;

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
         * @param historySyncTitle the history sync screen title.
         * @param historySyncSubtitle the history sync screen subtitle.
         */
        public Builder(
                AccountPickerBottomSheetStrings bottomSheetStrings,
                @NoAccountSigninMode int noAccountSigninMode,
                @WithAccountSigninMode int withAccountSigninMode,
                @HistorySyncConfig.OptInMode int historyOptInMode,
                String historySyncTitle,
                String historySyncSubtitle) {
            mBottomSheetStrings = bottomSheetStrings;
            mNoAccountSigninMode = noAccountSigninMode;
            mWithAccountSigninMode = withAccountSigninMode;
            mHistoryOptInMode = historyOptInMode;
            mHistorySyncTitle = historySyncTitle;
            mHistorySyncSubtitle = historySyncSubtitle;
        }

        /**
         * @param selectedCoreAccountId The account that should be displayed in the intermediate
         *     sign-in bottom sheet.
         */
        public Builder selectedCoreAccountId(CoreAccountId selectedCoreAccountId) {
            mSelectedCoreAccountId = selectedCoreAccountId;
            return this;
        }

        /**
         * @param selectedCoreAccountId In {@link WithAccountSigninMode#SEAMLESS_SIGNIN} mode, the
         *     bottom sheet is bypassed, and automatic sign-in with this account is triggered.
         */
        public Builder useSeamlessWithAccountSignin(CoreAccountId selectedCoreAccountId) {
            mSelectedCoreAccountId = selectedCoreAccountId;
            mWithAccountSigninMode = WithAccountSigninMode.SEAMLESS_SIGNIN;
            mShouldShowSigninSnackbar = true;
            return this;
        }

        /**
         * @param shouldShowSigninSnackbar If true, a snackbar will be shown after a successful
         *     sign-in, informing the user they have signed and allows the user to undo the sign-in
         *     by clicking the "Undo" button.
         */
        public Builder shouldShowSigninSnackbar(boolean shouldShowSigninSnackbar) {
            mShouldShowSigninSnackbar = shouldShowSigninSnackbar;
            return this;
        }

        /**
         * Builds the {@link BottomSheetSigninAndHistorySyncConfig} instance.
         *
         * <p>This method asserts that all necessary fields are correctly set before creating the
         * object.
         */
        public BottomSheetSigninAndHistorySyncConfig build() {
            final HistorySyncConfig historySyncConfig =
                    new HistorySyncConfig(
                            /* title= */ mHistorySyncTitle, /* subtitle= */ mHistorySyncSubtitle);
            return new BottomSheetSigninAndHistorySyncConfig(
                    mBottomSheetStrings,
                    historySyncConfig,
                    mNoAccountSigninMode,
                    mWithAccountSigninMode,
                    mHistoryOptInMode,
                    mSelectedCoreAccountId,
                    mShouldShowSigninSnackbar);
        }
    }

    private BottomSheetSigninAndHistorySyncConfig(
            AccountPickerBottomSheetStrings bottomSheetStrings,
            HistorySyncConfig historySyncConfig,
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @Nullable CoreAccountId selectedCoreAccountId,
            boolean shouldShowSigninSnackbar) {
        assert bottomSheetStrings != null;
        assert historySyncConfig != null;
        if (withAccountSigninMode == WithAccountSigninMode.SEAMLESS_SIGNIN) {
            assert selectedCoreAccountId != null
                    : "Must provide a nonnullable CoreAccountId for seamless sign-in flow";
            assert shouldShowSigninSnackbar
                    : "Must enable sign-in snackbar for seamless sign-in flow";
        }
        this.bottomSheetStrings = bottomSheetStrings;
        this.historySyncConfig = historySyncConfig;
        this.noAccountSigninMode = noAccountSigninMode;
        this.withAccountSigninMode = withAccountSigninMode;
        this.historyOptInMode = historyOptInMode;
        this.selectedCoreAccountId = selectedCoreAccountId;
        this.shouldShowSigninSnackbar = shouldShowSigninSnackbar;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof BottomSheetSigninAndHistorySyncConfig)) {
            return false;
        }

        BottomSheetSigninAndHistorySyncConfig other =
                (BottomSheetSigninAndHistorySyncConfig) object;
        return bottomSheetStrings.equals(other.bottomSheetStrings)
                && historySyncConfig.equals(other.historySyncConfig)
                && noAccountSigninMode == other.noAccountSigninMode
                && withAccountSigninMode == other.withAccountSigninMode
                && historyOptInMode == other.historyOptInMode
                && Objects.equals(selectedCoreAccountId, other.selectedCoreAccountId)
                && shouldShowSigninSnackbar == other.shouldShowSigninSnackbar;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                bottomSheetStrings,
                historySyncConfig,
                noAccountSigninMode,
                withAccountSigninMode,
                historyOptInMode,
                selectedCoreAccountId,
                shouldShowSigninSnackbar);
    }
}
