// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.text.TextUtils;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;

import java.util.Objects;

/** Class containing resources for the fullscreen sign-in view. */
@NullMarked
public final class FullscreenSigninConfig {
    public final String title;
    public final String subtitle;
    public final String dismissText;
    public final @DrawableRes int logoId;
    public final boolean shouldDisableSignin;
    public final @Nullable @SigninSurveyController.SigninSurveyType Integer signinSurveyType;
    public final @Nullable String selectedAccountEmail;

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
     * @param surveyType The survey type to use for the sign-in flow.
     * @param selectedAccountEmail the email of the account to auto-select in the sign-in flow.
     */
    public FullscreenSigninConfig(
            String title,
            String subtitle,
            String dismissText,
            @DrawableRes int logoId,
            boolean shouldDisableSignin,
            @Nullable @SigninSurveyController.SigninSurveyType Integer surveyType,
            @Nullable String selectedAccountEmail) {
        assert !TextUtils.isEmpty(title);
        assert !TextUtils.isEmpty(subtitle);
        assert !TextUtils.isEmpty(dismissText);
        this.title = title;
        this.subtitle = subtitle;
        this.dismissText = dismissText;
        this.logoId = logoId;
        this.shouldDisableSignin = shouldDisableSignin;
        this.signinSurveyType = surveyType;
        this.selectedAccountEmail = selectedAccountEmail;
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
                && shouldDisableSignin == other.shouldDisableSignin
                && Objects.equals(signinSurveyType, other.signinSurveyType)
                && Objects.equals(selectedAccountEmail, other.selectedAccountEmail);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                title,
                subtitle,
                dismissText,
                logoId,
                shouldDisableSignin,
                signinSurveyType,
                selectedAccountEmail);
    }
}
