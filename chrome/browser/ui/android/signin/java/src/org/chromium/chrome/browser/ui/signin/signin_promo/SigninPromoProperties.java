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
            ON_ACCOUNT_PICKER_CLICKED =
                    new PropertyModel.WritableObjectPropertyKey<>("on_account_picker_clicked");

    static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_DISMISS_BUTTON_CLICKED =
                    new PropertyModel.WritableObjectPropertyKey<>("on_dismiss_button_clicked");

    static final PropertyModel.WritableObjectPropertyKey<String> TITLE_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("title_text");

    static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("description_text");

    static final PropertyModel.WritableObjectPropertyKey<String> PRIMARY_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("primary_button_text");

    static final PropertyModel.WritableBooleanPropertyKey SHOULD_HIDE_DISMISS_BUTTON =
            new PropertyModel.WritableBooleanPropertyKey("should_hide_dismiss_button");

    static final PropertyModel.WritableBooleanPropertyKey SHOULD_SHOW_ACCOUNT_PICKER =
            new PropertyModel.WritableBooleanPropertyKey("should_show_account_picker");

    static final PropertyModel.WritableBooleanPropertyKey SHOULD_SHOW_HEADER_WITH_AVATAR =
            new PropertyModel.WritableBooleanPropertyKey("should_show_header_with_avatar");

    static final PropertyModel.WritableBooleanPropertyKey SHOULD_SHOW_LOADING_STATE =
            new PropertyModel.WritableBooleanPropertyKey("should_show_loading_state");

    static final PropertyModel.WritableIntPropertyKey SELECTED_ACCOUNT_VIEW_BACKGROUND =
            new PropertyModel.WritableIntPropertyKey("selected_account_view_background");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PROFILE_DATA,
                ON_PRIMARY_BUTTON_CLICKED,
                ON_ACCOUNT_PICKER_CLICKED,
                ON_DISMISS_BUTTON_CLICKED,
                TITLE_TEXT,
                DESCRIPTION_TEXT,
                PRIMARY_BUTTON_TEXT,
                SHOULD_HIDE_DISMISS_BUTTON,
                SHOULD_SHOW_ACCOUNT_PICKER,
                SHOULD_SHOW_HEADER_WITH_AVATAR,
                SHOULD_SHOW_LOADING_STATE,
                SELECTED_ACCOUNT_VIEW_BACKGROUND
            };

    private SigninPromoProperties() {}

    static PropertyModel createModel(
            @Nullable DisplayableProfileData profileData,
            Runnable onPrimaryButtonClicked,
            Runnable onAccountPickerClicked,
            Runnable onDismissButtonClicked,
            String titleString,
            String descriptionString,
            String primaryButtonString,
            boolean shouldHideDismissButton,
            boolean shouldShowAccountPicker,
            boolean shouldShowHeaderWithAvatar,
            boolean shouldShowLoadingState,
            int accountPickerBackground) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(PROFILE_DATA, profileData)
                .with(ON_PRIMARY_BUTTON_CLICKED, _ -> onPrimaryButtonClicked.run())
                .with(ON_ACCOUNT_PICKER_CLICKED, _ -> onAccountPickerClicked.run())
                .with(ON_DISMISS_BUTTON_CLICKED, _ -> onDismissButtonClicked.run())
                .with(TITLE_TEXT, titleString)
                .with(DESCRIPTION_TEXT, descriptionString)
                .with(PRIMARY_BUTTON_TEXT, primaryButtonString)
                .with(SHOULD_HIDE_DISMISS_BUTTON, shouldHideDismissButton)
                .with(SHOULD_SHOW_ACCOUNT_PICKER, shouldShowAccountPicker)
                .with(SHOULD_SHOW_HEADER_WITH_AVATAR, shouldShowHeaderWithAvatar)
                .with(SHOULD_SHOW_LOADING_STATE, shouldShowLoadingState)
                .with(SELECTED_ACCOUNT_VIEW_BACKGROUND, accountPickerBackground)
                .build();
    }
}
