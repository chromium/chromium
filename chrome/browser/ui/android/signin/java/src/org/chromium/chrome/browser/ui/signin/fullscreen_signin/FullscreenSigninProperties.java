// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.animation.Animator.AnimatorListener;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class FullscreenSigninProperties {
    static final ReadableObjectPropertyKey<OnClickListener> ON_SELECTED_ACCOUNT_CLICKED =
            new ReadableObjectPropertyKey<>("on_selected_account_clicked");
    static final WritableObjectPropertyKey<DisplayableProfileData> SELECTED_ACCOUNT_DATA =
            new WritableObjectPropertyKey<>("selected_account_data");

    // PropertyKey for the button |Continue as ...|
    static final ReadableObjectPropertyKey<OnClickListener> ON_CONTINUE_AS_CLICKED =
            new ReadableObjectPropertyKey<>("on_continue_as_clicked");

    // PropertyKey for the dismiss button
    static final ReadableObjectPropertyKey<OnClickListener> ON_DISMISS_CLICKED =
            new ReadableObjectPropertyKey<>("on_dismiss_clicked");

    // Is not initialized in #createModel(...) to avoid conflicting view changes with
    // ARE_NATIVE_AND_POLICY_LOADED. Will be set when |Continue as ...| is pressed.
    static final WritableBooleanPropertyKey SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT =
            new WritableBooleanPropertyKey("show_signin_progress_spinner_with_text");

    // Is not initialized in #createModel(...) to avoid conflicting view changes with
    // ARE_NATIVE_AND_POLICY_LOADED. Will be set when dismiss button is pressed.
    static final WritableBooleanPropertyKey SHOW_SIGNIN_PROGRESS_SPINNER =
            new WritableBooleanPropertyKey("show_signin_progress_spinner");

    static final WritableBooleanPropertyKey SHOW_INITIAL_LOAD_PROGRESS_SPINNER =
            new WritableBooleanPropertyKey("show_initial_load_progress_spinner");

    static final WritableBooleanPropertyKey SHOW_ENTERPRISE_MANAGEMENT_NOTICE =
            new WritableBooleanPropertyKey("show_enterprise_management_notice");

    static final WritableBooleanPropertyKey SHOW_ACCOUNT_SUPERVISION_NOTICE =
            new WritableBooleanPropertyKey("is_selected_account_supervised");

    static final WritableBooleanPropertyKey IS_SIGNIN_FORCED =
            new WritableBooleanPropertyKey("force_signin_enabled");

    static final WritableBooleanPropertyKey IS_SIGNIN_SUPPORTED =
            new WritableBooleanPropertyKey("is_signin_supported");

    static final WritableIntPropertyKey LOGO_DRAWABLE_ID =
            new WritableIntPropertyKey("logo_drawable_id");

    /** Profile picture after the user signs into FRE. Setting this also starts an animation. */
    static final WritableObjectPropertyKey<Drawable> PROFILE_PICTURE =
            new WritableObjectPropertyKey<>("profile_picture");

    /**
     * Whether the animation should be shown. We expect this to be true iff LOGO_DRAWABLE_ID == 0.
     */
    static final WritableBooleanPropertyKey SHOW_ANIMATION =
            new WritableBooleanPropertyKey("show_animation");

    /** Whether the animation should start playing. */
    static final WritableBooleanPropertyKey START_ANIMATION =
            new WritableBooleanPropertyKey("start_animation");

    /** A {@link AnimatorListener} to be attached to the sign-in animation. */
    static final WritableObjectPropertyKey<AnimatorListener> ANIMATOR_LISTENER =
            new WritableObjectPropertyKey<>("animator_listener");

    static final WritableObjectPropertyKey<String> TITLE_STRING =
            new WritableObjectPropertyKey<>("title_string");

    static final WritableObjectPropertyKey<String> SUBTITLE_STRING =
            new WritableObjectPropertyKey<>("subtitle_string");

    static final WritableObjectPropertyKey<String> DISMISS_BUTTON_STRING =
            new WritableObjectPropertyKey<>("dismiss_button_string");

    static final WritableObjectPropertyKey<CharSequence> FOOTER_STRING =
            new WritableObjectPropertyKey<>("footer_string");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ON_SELECTED_ACCOUNT_CLICKED,
                SELECTED_ACCOUNT_DATA,
                SHOW_ACCOUNT_SUPERVISION_NOTICE,
                ON_CONTINUE_AS_CLICKED,
                ON_DISMISS_CLICKED,
                SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT,
                SHOW_SIGNIN_PROGRESS_SPINNER,
                SHOW_INITIAL_LOAD_PROGRESS_SPINNER,
                SHOW_ENTERPRISE_MANAGEMENT_NOTICE,
                IS_SIGNIN_FORCED,
                IS_SIGNIN_SUPPORTED,
                LOGO_DRAWABLE_ID,
                PROFILE_PICTURE,
                SHOW_ANIMATION,
                START_ANIMATION,
                ANIMATOR_LISTENER,
                TITLE_STRING,
                SUBTITLE_STRING,
                DISMISS_BUTTON_STRING,
                FOOTER_STRING,
            };

    /** Creates a default model for FRE bottom group. */
    static PropertyModel createModel(
            Runnable onSelectedAccountClicked,
            Runnable onContinueAsClicked,
            Runnable onDismissClicked,
            boolean isSigninSupported,
            @DrawableRes int logoDrawableId,
            String titleString,
            String subtitleString,
            String dismissString,
            boolean showInitialLoadProgressSpinner) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ON_SELECTED_ACCOUNT_CLICKED, v -> onSelectedAccountClicked.run())
                .with(SELECTED_ACCOUNT_DATA, null)
                .with(SHOW_ACCOUNT_SUPERVISION_NOTICE, false)
                .with(ON_CONTINUE_AS_CLICKED, v -> onContinueAsClicked.run())
                .with(ON_DISMISS_CLICKED, v -> onDismissClicked.run())
                .with(SHOW_INITIAL_LOAD_PROGRESS_SPINNER, showInitialLoadProgressSpinner)
                .with(SHOW_ENTERPRISE_MANAGEMENT_NOTICE, false)
                .with(IS_SIGNIN_FORCED, false)
                .with(IS_SIGNIN_SUPPORTED, isSigninSupported)
                .with(LOGO_DRAWABLE_ID, logoDrawableId)
                .with(TITLE_STRING, titleString)
                .with(SUBTITLE_STRING, subtitleString)
                .with(DISMISS_BUTTON_STRING, dismissString)
                .with(FOOTER_STRING, null)
                .build();
    }

    private FullscreenSigninProperties() {}
}
