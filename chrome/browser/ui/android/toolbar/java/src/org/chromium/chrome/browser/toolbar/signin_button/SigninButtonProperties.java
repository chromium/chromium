// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The SigninButton model allows for interactions between the view and the controller. */
@NullMarked
final class SigninButtonProperties {

    // Indicates whether the signin button view should be displayed.
    public static final WritableBooleanPropertyKey SHOULD_SHOW_ON_PAGE =
            new WritableBooleanPropertyKey();

    // Indicates whether the signin button view is visible. This should only be true if
    // SHOULD_SHOW_ON_PAGE is also true and there is space to show the button.
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    // Indicates whether the signin text button should be used. If false, the avatar button
    // is used instead.
    public static final WritableBooleanPropertyKey USE_SIGNIN_TEXT_BUTTON =
            new WritableBooleanPropertyKey();

    // The image displayed within the avatar button, i.e. profile picture or generic account circle.
    public static final WritableObjectPropertyKey<Drawable> BUTTON_AVATAR =
            new WritableObjectPropertyKey<>();

    // The tint applied to the avatar button.
    public static final WritableObjectPropertyKey<ColorStateList> AVATAR_TINT =
            new WritableObjectPropertyKey<>();

    // The click listener for the signin button to handle interactions with the button.
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>("on_click");

    // The content description for the avatar button.
    public static final WritableObjectPropertyKey<String> AVATAR_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    // Indicates whether the signin button is enabled.
    public static final WritableBooleanPropertyKey IS_ENABLED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                SHOULD_SHOW_ON_PAGE,
                IS_VISIBLE,
                USE_SIGNIN_TEXT_BUTTON,
                BUTTON_AVATAR,
                AVATAR_TINT,
                ON_CLICK,
                AVATAR_CONTENT_DESCRIPTION,
                IS_ENABLED
            };
}
