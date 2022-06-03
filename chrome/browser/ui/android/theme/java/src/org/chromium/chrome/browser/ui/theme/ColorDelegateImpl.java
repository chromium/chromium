// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.theme;

import android.app.Activity;

/** A no-op implementation of {@link ColorDelegate}. */
public class ColorDelegateImpl implements ColorDelegate {
    @Override
    public void applyDynamicColorsIfAvailable(Activity activity) {}
}