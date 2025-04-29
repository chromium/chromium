// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.R;

import java.util.Objects;

/* Class containing IDs of resources for the fullscreen sign-in view. */
@NullMarked
public final class FullscreenSigninConfig {
    public final @StringRes int titleId;
    public final @StringRes int subtitleId;
    public final @StringRes int dismissTextId;
    public final @DrawableRes int logoId;
    public final boolean shouldDisableSignin;

    /**
     * Constructor of FullscreenSigninConfig using default values.
     *
     * @param shouldDisableSignin Whether the sign-in should always be disabled for sign-in flows
     *     started by the caller. The sign-in screen will show a generic title and a continue
     *     button.
     */
    public FullscreenSigninConfig(boolean shouldDisableSignin) {
        this(
                /* titleId= */ R.string.signin_fre_title,
                /* subtitleId= */ R.string.signin_fre_subtitle,
                /* dismissTextId= */ R.string.signin_fre_dismiss_button,
                /* logoId= */ 0,
                /* shouldDisableSignin= */ shouldDisableSignin);
    }

    /**
     * Constructor of FullscreenSigninConfig.
     *
     * @param titleId the resource ID of the title string.
     * @param subtitleId the resource ID of the subtitle string.
     * @param dismissTextId the resource ID of the dismiss button string.
     * @param logoId the resource ID of the logo drawable. Can be set to 0 to use the default
     *     sign-in logo.
     * @param shouldDisableSignin Whether the sign-in should always be disabled for sign-in flows
     *     started by the caller. The sign-in screen will show a generic title and a continue
     *     button.
     */
    public FullscreenSigninConfig(
            @StringRes int titleId,
            @StringRes int subtitleId,
            @StringRes int dismissTextId,
            @DrawableRes int logoId,
            boolean shouldDisableSignin) {
        this.titleId = titleId;
        this.subtitleId = subtitleId;
        this.dismissTextId = dismissTextId;
        this.logoId = logoId;
        this.shouldDisableSignin = shouldDisableSignin;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof FullscreenSigninConfig)) {
            return false;
        }

        FullscreenSigninConfig other = (FullscreenSigninConfig) object;
        return titleId == other.titleId
                && subtitleId == other.subtitleId
                && dismissTextId == other.dismissTextId
                && logoId == other.logoId
                && shouldDisableSignin == other.shouldDisableSignin;
    }

    @Override
    public int hashCode() {
        return Objects.hash(titleId, subtitleId, dismissTextId, logoId, shouldDisableSignin);
    }
}
