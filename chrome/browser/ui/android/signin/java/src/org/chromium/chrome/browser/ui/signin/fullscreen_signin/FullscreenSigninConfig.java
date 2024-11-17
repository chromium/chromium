// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.signin.R;

/* Class containing IDs of resources for the fullscreen sign-in view. */
public final class FullscreenSigninConfig implements Parcelable {
    public final @StringRes int titleId;
    public final @StringRes int subtitleId;
    public final @StringRes int dismissTextId;
    public final @DrawableRes int logoId;

    public static final Parcelable.Creator<FullscreenSigninConfig> CREATOR =
            new Parcelable.Creator<FullscreenSigninConfig>() {
                @Override
                public FullscreenSigninConfig createFromParcel(Parcel in) {
                    return new FullscreenSigninConfig(in);
                }

                @Override
                public FullscreenSigninConfig[] newArray(int size) {
                    return new FullscreenSigninConfig[size];
                }
            };

    public FullscreenSigninConfig() {
        this(
                /* titleId= */ R.string.signin_fre_title,
                /* subtitleId= */ R.string.signin_fre_subtitle,
                /* dismissTextId= */ R.string.signin_fre_dismiss_button,
                /* logoId= */ R.drawable.fre_product_logo);
    }

    public FullscreenSigninConfig(
            @StringRes int titleId,
            @StringRes int subtitleId,
            @StringRes int dismissTextId,
            @DrawableRes int logoId) {
        this.titleId = titleId;
        this.subtitleId = subtitleId;
        this.dismissTextId = dismissTextId;
        this.logoId = logoId;
    }

    private FullscreenSigninConfig(Parcel in) {
        this(
                /* titleId= */ in.readInt(),
                /* subtitleId= */ in.readInt(),
                /* dismissTextId= */ in.readInt(),
                /* logoId= */ in.readInt());
    }

    /** Implements {@link Parcelable} */
    @Override
    public int describeContents() {
        return 0;
    }

    /** Implements {@link Parcelable} */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(titleId);
        out.writeInt(subtitleId);
        out.writeInt(dismissTextId);
        out.writeInt(logoId);
    }
}
