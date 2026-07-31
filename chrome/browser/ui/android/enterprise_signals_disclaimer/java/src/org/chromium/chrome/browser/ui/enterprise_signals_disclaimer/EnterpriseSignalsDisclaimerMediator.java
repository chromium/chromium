// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Objects;

/**
 * Mediator for the enterprise signals disclaimer.
 *
 * <p>This disclaimer is shown on browser startup to managed users missing the consent for
 * collecting the device signals. The dialog offers two choices: Accept or Decline. Accepting the
 * dialog dismisses it and allows the user to proceed with using the browser while also marking the
 * consent as granted. Dismissing the dialog with a gesture or explicitly clicking 'Sign out' will
 * sign the user out.
 */
@NullMarked
class EnterpriseSignalsDisclaimerMediator implements ProfileDataCache.Observer {
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private final AccountInfo mPrimaryAccount;

    EnterpriseSignalsDisclaimerMediator(Context context, IdentityManager identityManager) {
        mPrimaryAccount = Objects.requireNonNull(identityManager.getPrimaryAccountInfo());

        // Puts the badge in the bottom right corner of the profile picture.
        BadgeConfig badgeConfig =
                BadgeConfig.create(R.drawable.enterprise_badge_icon)
                        .withBadgeSize(R.dimen.enterprise_signals_disclaimer_badge_size)
                        .withBorderSize(R.dimen.enterprise_signals_disclaimer_badge_border_size)
                        .withXPosition(R.dimen.enterprise_signals_disclaimer_badge_x_position)
                        .withYPosition(R.dimen.enterprise_signals_disclaimer_badge_y_position)
                        .build(context);
        mProfileDataCache =
                ProfileDataCache.createWithoutBadge(
                        context,
                        identityManager,
                        R.dimen.enterprise_signals_disclaimer_profile_picture_size);
        mProfileDataCache.setBadge(badgeConfig);

        mModel =
                new PropertyModel.Builder(EnterpriseSignalsDisclaimerProperties.ALL_KEYS)
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_PICTURE,
                                mProfileDataCache.getById(mPrimaryAccount.getId()).getImage())
                        .with(
                                EnterpriseSignalsDisclaimerProperties.TITLE,
                                context.getString(R.string.enterprise_signals_disclaimer_title))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DESCRIPTION,
                                context.getString(
                                        R.string.enterprise_signals_disclaimer_description))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_TITLE,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_profile_information_title))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_DETAILS,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_profile_information_details))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_TITLE,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_device_information_title))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_DETAILS,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_device_information_details))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ACCEPT_BUTTON_TEXT,
                                context.getString(
                                        R.string.enterprise_signals_disclaimer_accept_button_text))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.CANCEL_BUTTON_TEXT,
                                context.getString(
                                        R.string.enterprise_signals_disclaimer_cancel_button_text))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED,
                                v -> onAccept())
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED,
                                v -> onCancel())
                        .build();

        mProfileDataCache.addObserver(this);
    }

    /**
     * Returns the PropertyModel managed by this mediator.
     *
     * @return The PropertyModel.
     */
    public PropertyModel getModel() {
        return mModel;
    }

    /** Dismisses the dialog and marks the device signals collection consent as granted. */
    void onAccept() {}

    /**
     * Signs the user out. This is also triggered when the dialog is dismissed by a back press,
     * sliding down or tapping outside of the dialog area.
     */
    void onCancel() {}

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        if (profileData.getAccountId().equals(mPrimaryAccount.getId())) {
            mModel.set(
                    EnterpriseSignalsDisclaimerProperties.PROFILE_PICTURE, profileData.getImage());
        }
    }

    void destroy() {
        mProfileDataCache.removeObserver(this);
    }
}
