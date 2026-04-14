// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewStructure;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A View which has displays for various situations (user sign-in state, whether NTP is shown, etc.)
 */
@NullMarked
final class SigninButtonView extends FrameLayout {
    private ChromeImageButton mAvatarButton;
    private ButtonCompat mSigninTextButton;

    public SigninButtonView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mAvatarButton = findViewById(R.id.avatar_button);
        mSigninTextButton = findViewById(R.id.signin_text_button);
    }

    // TODO(crbug.com/501318669): Remove after investigating further to understand the crashes.
    @Override
    public void dispatchProvideStructure(ViewStructure structure) {
        try {
            super.dispatchProvideStructure(structure);
        } catch (IndexOutOfBoundsException e) {
            // Absorb the layout bug caused by traversal after platform translation (b/394874193).
        }
    }

    ChromeImageButton getAvatarButton() {
        return mAvatarButton;
    }

    ButtonCompat getSigninTextButton() {
        return mSigninTextButton;
    }
}
