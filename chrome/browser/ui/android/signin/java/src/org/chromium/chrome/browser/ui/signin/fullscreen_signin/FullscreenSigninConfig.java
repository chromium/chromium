// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.text.TextUtils;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Class containing resources for the fullscreen sign-in view. */
@NullMarked
public final class FullscreenSigninConfig {
    public final String title;
    public final String subtitle;
    public final String dismissText;
    public final @DrawableRes int logoId;
    public final boolean shouldDisableSignin;

    public static final String DISMISS_TEXT_NOT_INITIALIZED = "";

    /**
     * Constructor of FullscreenSigninConfig.
     *
     * @param title The title string.
     * @param subtitle The subtitle string.
     * @param dismissText The dismiss button string.
     * @param logoId the resource ID of the logo drawable. Can be set to 0 to use the default
     *     sign-in logo.
     * @param shouldDisableSignin Whether the sign-in should always be disabled for sign-in flows
     *     started by the caller. The sign-in screen will show a generic title and a continue
     *     button.
     */
    public FullscreenSigninConfig(
            String title,
            String subtitle,
            String dismissText,
            @DrawableRes int logoId,
            boolean shouldDisableSignin) {
        assert !TextUtils.isEmpty(title);
        assert !TextUtils.isEmpty(subtitle);
        // TODO(crbug.com/464416507): Restore the assert that dismissText is not empty once
        // the FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT flag is cleaned up.
        this.title = title;
        this.subtitle = subtitle;
        this.dismissText = dismissText;
        this.logoId = logoId;
        this.shouldDisableSignin = shouldDisableSignin;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof FullscreenSigninConfig)) {
            return false;
        }

        FullscreenSigninConfig other = (FullscreenSigninConfig) object;
        return Objects.equals(title, other.title)
                && Objects.equals(subtitle, other.subtitle)
                && Objects.equals(dismissText, other.dismissText)
                && logoId == other.logoId
                && shouldDisableSignin == other.shouldDisableSignin;
    }

    @Override
    public int hashCode() {
        return Objects.hash(title, subtitle, dismissText, logoId, shouldDisableSignin);
    }
}
