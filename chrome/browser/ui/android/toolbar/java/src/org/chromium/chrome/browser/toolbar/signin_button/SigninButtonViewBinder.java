// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;

/** The ViewBinder connecting SigninButtonProperties and SigninButtonView. */
@NullMarked
final class SigninButtonViewBinder {
    public static void bind(PropertyModel model, SigninButtonView view, PropertyKey propertyKey) {
        if (SigninButtonProperties.BUTTON_AVATAR.equals(propertyKey)) {
            ChromeImageButton avatarButton = view.getAvatarButton();
            avatarButton.setImageDrawable(model.get(SigninButtonProperties.BUTTON_AVATAR));
        } else if (SigninButtonProperties.SHOW_BUTTON.equals(propertyKey)) {
            view.setVisibility(
                    model.get(SigninButtonProperties.SHOW_BUTTON) ? View.VISIBLE : View.GONE);
        } else if (SigninButtonProperties.SHOW_AVATAR.equals(propertyKey)) {
            ChromeImageButton avatarButton = view.getAvatarButton();
            avatarButton.setVisibility(
                    model.get(SigninButtonProperties.SHOW_AVATAR) ? View.VISIBLE : View.GONE);
        }
    }
}
