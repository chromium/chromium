// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;

/**
 * Class containing IDs of resources for the fullscreen sign-in view and the history sync opt-in
 * view.
 */
public final class FullscreenSigninAndHistorySyncConfig implements Parcelable {
    public final FullscreenSigninConfig signinConfig;
    public final HistorySyncConfig historySyncConfig;
    public final @HistorySyncConfig.OptInMode int historyOptInMode;

    /**
     * Builder for {@link FullscreenSigninAndHistorySyncConfig} which contains resource IDs for the
     * sign-in and history sync fullscreen views.
     */
    public static class Builder {
        private @StringRes int mSigninTitleId = R.string.signin_fre_title;
        private @StringRes int mSigninSubtitleId = R.string.signin_fre_subtitle;
        private @StringRes int mSigninDismissTextId = R.string.signin_fre_dismiss_button;
        private @DrawableRes int mSigninLogoId = R.drawable.fre_product_logo;
        private @StringRes int mHistorySyncTitleId = R.string.history_sync_title;
        private @StringRes int mHistorySyncSubtitleId = R.string.history_sync_subtitle;
        private @HistorySyncConfig.OptInMode int mHistoryOptInMode =
                HistorySyncConfig.OptInMode.OPTIONAL;

        public Builder() {}

        public Builder signinTitleId(@StringRes int signinTitleId) {
            assert signinTitleId != 0;
            mSigninTitleId = signinTitleId;
            return this;
        }

        public Builder signinSubtitleId(@StringRes int signinSubtitleId) {
            assert signinSubtitleId != 0;
            mSigninSubtitleId = signinSubtitleId;
            return this;
        }

        public Builder signinDismissTextId(@StringRes int signinDismissTextId) {
            assert signinDismissTextId != 0;
            mSigninDismissTextId = signinDismissTextId;
            return this;
        }

        public Builder signinLogoId(@DrawableRes int signinLogoId) {
            assert signinLogoId != 0;
            mSigninLogoId = signinLogoId;
            return this;
        }

        public Builder historySyncTitleId(@StringRes int historySyncTitleId) {
            assert historySyncTitleId != 0;
            mHistorySyncTitleId = historySyncTitleId;
            return this;
        }

        public Builder historySyncSubtitleId(@StringRes int historySyncSubtitleId) {
            assert historySyncSubtitleId != 0;
            mHistorySyncSubtitleId = historySyncSubtitleId;
            return this;
        }

        public Builder historyOptInMode(@HistorySyncConfig.OptInMode int historyOptInMode) {
            mHistoryOptInMode = historyOptInMode;
            return this;
        }

        public FullscreenSigninAndHistorySyncConfig build() {
            final FullscreenSigninConfig signinConfig =
                    new FullscreenSigninConfig(
                            /* titleId= */ mSigninTitleId,
                            /* subtitleId= */ mSigninSubtitleId,
                            /* dismissTextId= */ mSigninDismissTextId,
                            /* logoId= */ mSigninLogoId);
            final HistorySyncConfig historySyncConfig =
                    new HistorySyncConfig(
                            /* titleId= */ mHistorySyncTitleId,
                            /* subtitleId= */ mHistorySyncSubtitleId);

            return new FullscreenSigninAndHistorySyncConfig(
                    signinConfig, historySyncConfig, mHistoryOptInMode);
        }
    }

    public static final Parcelable.Creator<FullscreenSigninAndHistorySyncConfig> CREATOR =
            new Parcelable.Creator<FullscreenSigninAndHistorySyncConfig>() {
                @Override
                public FullscreenSigninAndHistorySyncConfig createFromParcel(Parcel in) {
                    return new FullscreenSigninAndHistorySyncConfig(in);
                }

                @Override
                public FullscreenSigninAndHistorySyncConfig[] newArray(int size) {
                    return new FullscreenSigninAndHistorySyncConfig[size];
                }
            };

    private FullscreenSigninAndHistorySyncConfig(
            FullscreenSigninConfig signinConfig,
            HistorySyncConfig historySyncConfig,
            @HistorySyncConfig.OptInMode int historyOptInMode) {
        this.signinConfig = signinConfig;
        this.historySyncConfig = historySyncConfig;
        this.historyOptInMode = historyOptInMode;
    }

    private FullscreenSigninAndHistorySyncConfig(Parcel in) {
        this(
                in.readParcelable(FullscreenSigninConfig.class.getClassLoader()),
                in.readParcelable(HistorySyncConfig.class.getClassLoader()),
                /* historyOptInMode= */ in.readInt());
    }

    /** Implements {@link Parcelable} */
    @Override
    public int describeContents() {
        return 0;
    }

    /** Implements {@link Parcelable} */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeParcelable(signinConfig, 0);
        out.writeParcelable(historySyncConfig, 0);
        out.writeInt(historyOptInMode);
    }
}
