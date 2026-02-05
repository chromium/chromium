// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Holds parameters for the request access button. */
@NullMarked
@JNINamespace("extensions")
public class RequestAccessButtonParams {
    private final String[] mExtensionIds;
    private final String mTooltipText;

    @CalledByNative
    public RequestAccessButtonParams(
            @JniType("std::vector<std::string>") String[] extensionIds,
            @JniType("std::u16string") String tooltipText) {
        mExtensionIds = extensionIds;
        mTooltipText = tooltipText;
    }

    public String[] getExtensionIds() {
        return mExtensionIds;
    }

    public String getTooltipText() {
        return mTooltipText;
    }
}
