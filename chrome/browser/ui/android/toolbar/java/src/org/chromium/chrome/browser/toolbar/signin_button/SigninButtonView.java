// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import android.content.Context;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;

/**
 * TODO(crbug.com/475816843): Implement View which has displays for various situations (user sign-in
 * state, whether NTP is shown, etc.)
 */
@NullMarked
final class SigninButtonView extends FrameLayout {

    public SigninButtonView(Context context) {
        super(context);
    }
}
