// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
final class SigninPromoProperties {
    static final PropertyModel.WritableObjectPropertyKey<@Nullable DisplayableProfileData>
            PROFILE_DATA = new PropertyModel.WritableObjectPropertyKey<>("profile_data");

    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_PRIMARY_BUTTON_CLICKED =
                    new PropertyModel.WritableObjectPropertyKey<>("on_primary_button_clicked");

    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_SECONDARY_BUTTON_CLICKED =
                    new PropertyModel.WritableObjectPropertyKey<>("on_secondary_button_clicked");

    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_DISMISS_BUTTON_CLICKED =
                    new PropertyModel.WritableObjectPropertyKey<>("on_dismiss_button_clicked");

    static final PropertyModel.WritableObjectPropertyKey<String> TITLE_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("title_text");

    static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("description_text");

    static final PropertyModel.WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("primary_button_text");

    static final PropertyModel.WritableObjectPropertyKey<String> SECONDARY_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("secondary_button_text");

    static final PropertyModel.WritableBooleanPropertyKey SHOULD_HIDE_SECONDARY_BUTTON =
            new PropertyModel.WritableBooleanPropertyKey("should_suppress_secondary_button");

    static final PropertyModel.WritableBooleanPropertyKey SHOULD_HIDE_DISMISS_BUTTON =
            new PropertyModel.WritableBooleanPropertyKey("should_hide_dismiss_button");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PROFILE_DATA,
                ON_PRIMARY_BUTTON_CLICKED,
                ON_SECONDARY_BUTTON_CLICKED,
                ON_DISMISS_BUTTON_CLICKED,
                TITLE_TEXT,
                DESCRIPTION_TEXT,
                PRIMARY_BUTTON_TEXT,
                SECONDARY_BUTTON_TEXT,
                SHOULD_HIDE_SECONDARY_BUTTON,
                SHOULD_HIDE_DISMISS_BUTTON
            };

    private SigninPromoProperties() {}

    static PropertyModel createModel(
            @Nullable DisplayableProfileData profileData,
            Runnable onPrimaryButtonClicked,
            Runnable onSecondaryButtonClicked,
            Runnable onDismissButtonClicked,
            String titleString,
            String descriptionString,
            String primaryButtonString,
            String secondaryButtonString,
            boolean shouldSuppressSecondaryButton,
            boolean shouldHideDismissButton) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(PROFILE_DATA, profileData)
                .with(ON_PRIMARY_BUTTON_CLICKED, (unusedView) -> onPrimaryButtonClicked.run())
                .with(ON_SECONDARY_BUTTON_CLICKED, (unusedView) -> onSecondaryButtonClicked.run())
                .with(ON_DISMISS_BUTTON_CLICKED, (unusedView) -> onDismissButtonClicked.run())
                .with(TITLE_TEXT, titleString)
                .with(DESCRIPTION_TEXT, descriptionString)
                .with(PRIMARY_BUTTON_TEXT, primaryButtonString)
                .with(SECONDARY_BUTTON_TEXT, secondaryButtonString)
                .with(SHOULD_HIDE_SECONDARY_BUTTON, shouldSuppressSecondaryButton)
                .with(SHOULD_HIDE_DISMISS_BUTTON, shouldHideDismissButton)
                .build();
    }
}
