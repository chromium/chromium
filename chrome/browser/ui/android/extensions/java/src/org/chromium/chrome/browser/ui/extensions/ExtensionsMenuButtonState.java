// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Holds the visual state for the extensions menu button. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsMenuButtonState {
    private final String mTooltip;
    private final String mAccessibleText;
    private final @Nullable Bitmap mIcon;

    @CalledByNative
    public ExtensionsMenuButtonState(
            @JniType("std::string") String tooltip,
            @JniType("std::string") String accessibleText,
            @Nullable Bitmap icon) {
        mTooltip = tooltip;
        mAccessibleText = accessibleText;
        mIcon = icon;
    }

    public String getTooltip() {
        return mTooltip;
    }

    public String getAccessibleText() {
        return mAccessibleText;
    }

    public @Nullable Bitmap getIcon() {
        return mIcon;
    }
}
