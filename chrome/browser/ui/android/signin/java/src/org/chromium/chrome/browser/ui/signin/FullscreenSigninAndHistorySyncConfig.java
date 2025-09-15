// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;

import java.util.Objects;

/** Class containing resources for the fullscreen sign-in view and the history sync opt-in view. */
@NullMarked
public final class FullscreenSigninAndHistorySyncConfig {
    public final FullscreenSigninConfig signinConfig;
    public final HistorySyncConfig historySyncConfig;
    public final @HistorySyncConfig.OptInMode int historyOptInMode;

    /**
     * Builder for {@link FullscreenSigninAndHistorySyncConfig} which contains resources for the
     * sign-in and history sync fullscreen views.
     */
    public static class Builder {
        private final String mSigninTitle;
        private final String mSigninSubtitle;
        private final String mSigninDismissText;
        private @DrawableRes int mSigninLogoId;
        private boolean mShouldDisableSignin;
        private final String mHistorySyncTitle;
        private final String mHistorySyncSubtitle;
        private @HistorySyncConfig.OptInMode int mHistoryOptInMode =
                HistorySyncConfig.OptInMode.OPTIONAL;

        public Builder(
                String signinTitle,
                String signinSubtitle,
                String signinDismissText,
                String historySyncTitle,
                String historySyncSubtitle) {
            mSigninTitle = signinTitle;
            mSigninSubtitle = signinSubtitle;
            mSigninDismissText = signinDismissText;
            mHistorySyncTitle = historySyncTitle;
            mHistorySyncSubtitle = historySyncSubtitle;
        }

        // Set the drawable id of the sign-in screen logo. Should not be 0.
        public Builder signinLogoId(@DrawableRes int signinLogoId) {
            // TODO(crbug.com/390418475): Add assert to ensure it's not 0 once default null value
            // will be removed.
            mSigninLogoId = signinLogoId;
            return this;
        }

        // Set whether sign-in should be disabled. See {@link FullscreenSigninConfig}
        public Builder shouldDisableSignin(boolean shouldDisableSignin) {
            mShouldDisableSignin = shouldDisableSignin;
            return this;
        }

        public Builder historyOptInMode(@HistorySyncConfig.OptInMode int historyOptInMode) {
            mHistoryOptInMode = historyOptInMode;
            return this;
        }

        public FullscreenSigninAndHistorySyncConfig build() {
            final FullscreenSigninConfig signinConfig =
                    new FullscreenSigninConfig(
                            /* title= */ mSigninTitle,
                            /* subtitle= */ mSigninSubtitle,
                            /* dismissText= */ mSigninDismissText,
                            /* logoId= */ mSigninLogoId,
                            /* shouldDisableSignin= */ mShouldDisableSignin);
            final HistorySyncConfig historySyncConfig =
                    new HistorySyncConfig(
                            /* title= */ mHistorySyncTitle, /* subtitle= */ mHistorySyncSubtitle);

            return new FullscreenSigninAndHistorySyncConfig(
                    signinConfig, historySyncConfig, mHistoryOptInMode);
        }
    }

    private FullscreenSigninAndHistorySyncConfig(
            FullscreenSigninConfig signinConfig,
            HistorySyncConfig historySyncConfig,
            @HistorySyncConfig.OptInMode int historyOptInMode) {
        this.signinConfig = signinConfig;
        this.historySyncConfig = historySyncConfig;
        this.historyOptInMode = historyOptInMode;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof FullscreenSigninAndHistorySyncConfig)) {
            return false;
        }

        FullscreenSigninAndHistorySyncConfig other = (FullscreenSigninAndHistorySyncConfig) object;
        return signinConfig.equals(other.signinConfig)
                && historySyncConfig.equals(other.historySyncConfig)
                && historyOptInMode == other.historyOptInMode;
    }

    @Override
    public int hashCode() {
        return Objects.hash(signinConfig, historySyncConfig, historyOptInMode);
    }
}
