// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator.Delegate;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * State-machine implementation of {@link FullscreenSigninCoordinator.Mediator}, behind {@link
 * org.chromium.components.signin.SigninFeatures#FULLSCREEN_SIGNIN_MEDIATOR_STATE_MACHINE}.
 */
@NullMarked
class FullscreenSigninMediator implements FullscreenSigninCoordinator.Mediator {
    private final Context mContext;
    private final @SigninAccessPoint int mAccessPoint;
    private final PropertyModel mModel;

    private FullscreenSigninConfig mConfig;
    private boolean mDestroyed;

    FullscreenSigninMediator(
            Context context,
            ModalDialogManager modalDialogManager,
            Delegate delegate,
            PrivacyPreferencesManager privacyPreferencesManager,
            FullscreenSigninConfig config,
            @SigninAccessPoint int accessPoint) {
        mContext = context;
        mConfig = config;
        mAccessPoint = accessPoint;

        mModel =
                FullscreenSigninProperties.createModel(
                        this::onSelectedAccountClicked,
                        this::onContinueAsClicked,
                        this::onDismissClicked,
                        ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                                && !mConfig.shouldDisableSignin,
                        mConfig.logoId,
                        mContext.getString(R.string.fre_welcome),
                        mConfig.subtitle,
                        mConfig.dismissText,
                        /* showInitialLoadProgressSpinner= */ true);

        SigninMetricsUtils.logSigninStarted(mAccessPoint);
    }

    @Override
    public PropertyModel getModel() {
        return mModel;
    }

    @Override
    public void destroy() {
        assert !mDestroyed;
        mDestroyed = true;
    }

    @Override
    public void reset() {
        // TODO(crbug.com/579875669): Clear the sign-in spinners and animation when the user comes
        // back to this screen.
    }

    @Override
    public boolean isContinueOrDismissClicked() {
        // TODO(crbug.com/579875669): Return whether sign-in or dismissal is in progress.
        return false;
    }

    @Override
    public void onAccountAdded(String accountEmail) {
        // TODO(crbug.com/579875669): Select the newly added account.
    }

    // TODO(crbug.com/486190792): Remove once FREStringChanges feature is launched.
    @Override
    public void setConfig(FullscreenSigninConfig config) {
        mConfig = config;
    }

    @Override
    public void onAddAccountCanceled() {
        // TODO(crbug.com/579875669): Handle the add-account flow being canceled.
    }

    @Override
    public void onDeviceLockCanceled() {
        // TODO(crbug.com/579875669): Let the user continue after device lock is canceled.
    }

    @Override
    public void proceedWithSignIn() {
        // TODO(crbug.com/579875669): Sign in with the selected account.
    }

    @Override
    public void dismiss() {
        // TODO(crbug.com/579875669): Dismiss the screen without signing in.
    }

    @Override
    public Drawable getProfilePictureForTesting() {
        return mModel.get(FullscreenSigninProperties.PROFILE_PICTURE);
    }

    @Override
    public void setStartAnimationForTesting(boolean start) {
        mModel.set(FullscreenSigninProperties.START_ANIMATION, start);
    }

    @Override
    public @Nullable BadgeConfig getSigninAnimationBadgeConfigForTesting() {
        // TODO(crbug.com/579875669): Return the selected account's sign-in animation badge.
        return null;
    }

    @Override
    public @Nullable BadgeConfig getContinueButtonBadgeConfigForTesting() {
        // TODO(crbug.com/579875669): Return the selected account's continue button badge.
        return null;
    }

    private void onSelectedAccountClicked() {
        // TODO(crbug.com/579875669): Open the account picker.
    }

    private void onContinueAsClicked() {
        // TODO(crbug.com/579875669): Start signing in with the selected account.
    }

    private void onDismissClicked() {
        // TODO(crbug.com/579875669): Start dismissing the screen.
    }
}
