// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

final class SigninPromoProperties {
    static final PropertyModel.WritableObjectPropertyKey<DisplayableProfileData> PROFILE_DATA =
            new PropertyModel.WritableObjectPropertyKey<>("profile_data");

    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_PRIMARY_BUTTON_CLICKED =
                    new PropertyModel.WritableObjectPropertyKey<>("on_accept_clicked");

    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_SECONDARY_BUTTON_CLICKED =
                    new PropertyModel.WritableObjectPropertyKey<>("on_decline_clicked");

    static final PropertyModel.ReadableIntPropertyKey TITLE_STRING_ID =
            new PropertyModel.ReadableIntPropertyKey("title_string_id");

    static final PropertyModel.ReadableIntPropertyKey DESCRIPTION_STRING_ID =
            new PropertyModel.ReadableIntPropertyKey("description_string_id");

    static final PropertyModel.WritableBooleanPropertyKey SHOULD_HIDE_SECONDARY_BUTTON =
            new PropertyModel.WritableBooleanPropertyKey("should_suppress_secondary_button");

    static final PropertyModel.ReadableBooleanPropertyKey SHOULD_HIDE_DISMISS_BUTTON =
            new PropertyModel.ReadableBooleanPropertyKey("should_hide_dismiss_button");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PROFILE_DATA,
                ON_PRIMARY_BUTTON_CLICKED,
                ON_SECONDARY_BUTTON_CLICKED,
                TITLE_STRING_ID,
                DESCRIPTION_STRING_ID,
                SHOULD_HIDE_SECONDARY_BUTTON,
                SHOULD_HIDE_DISMISS_BUTTON
            };

    private SigninPromoProperties() {}

    static PropertyModel createModel(
            DisplayableProfileData profileData,
            View.OnClickListener onAcceptClicked,
            View.OnClickListener onDeclineClicked,
            @StringRes int titleStringId,
            @StringRes int descriptionStringId,
            boolean shouldSuppressSecondaryButton,
            boolean shouldHideDismissButton) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(PROFILE_DATA, profileData)
                .with(ON_PRIMARY_BUTTON_CLICKED, onAcceptClicked)
                .with(ON_SECONDARY_BUTTON_CLICKED, onDeclineClicked)
                .with(TITLE_STRING_ID, titleStringId)
                .with(DESCRIPTION_STRING_ID, descriptionStringId)
                .with(SHOULD_HIDE_SECONDARY_BUTTON, shouldSuppressSecondaryButton)
                .with(SHOULD_HIDE_DISMISS_BUTTON, shouldHideDismissButton)
                .build();
    }
}
