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
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.widget.ButtonCompat;

/**
 * A View which has displays for various situations (user sign-in state, whether NTP is shown, etc.)
 */
@NullMarked
final class SigninButtonView extends FrameLayout {
    private ListMenuButton mAvatarButton;
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

    // TODO(crbug.com/501318669): Remove once the underlying Android layout bug is fixed.
    @Override
    public void dispatchProvideStructure(ViewStructure structure) {
        try {
            super.dispatchProvideStructure(structure);
        } catch (IndexOutOfBoundsException e) {
            // Absorb the layout bug caused by traversal after platform translation (b/394874193).
        }
    }

    // TODO(crbug.com/501318669): Remove once the underlying Android layout bug is fixed.
    @Override
    public void dispatchProvideAutofillStructure(ViewStructure structure, int flags) {
        try {
            super.dispatchProvideAutofillStructure(structure, flags);
        } catch (IndexOutOfBoundsException e) {
            // Absorb the layout bug caused by traversal after platform translation (b/394874193).
        }
    }

    ListMenuButton getAvatarButton() {
        return mAvatarButton;
    }

    ButtonCompat getSigninTextButton() {
        return mSigninTextButton;
    }
}
