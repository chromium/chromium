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
    public static final WritableBooleanPropertyKey SHOW_BUTTON = new WritableBooleanPropertyKey();

    // Indicates whether the inner signin avatar button should be displayed.
    public static final WritableBooleanPropertyKey SHOW_AVATAR = new WritableBooleanPropertyKey();

    // The image displayed within the avatar button, i.e. profile picture or generic account circle.
    public static final WritableObjectPropertyKey<Drawable> BUTTON_AVATAR =
            new WritableObjectPropertyKey<>();

    // The tint applied to the avatar button.
    public static final WritableObjectPropertyKey<ColorStateList> AVATAR_TINT =
            new WritableObjectPropertyKey<>();

    // The click listener for the signin button to handle interactions with the button.
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>("on_click");

    // The content description for the signin button.
    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                SHOW_BUTTON, SHOW_AVATAR, BUTTON_AVATAR, AVATAR_TINT, ON_CLICK, CONTENT_DESCRIPTION
            };
}
