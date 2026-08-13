// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.customize_chrome;

import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/** Helper for Customize Chrome Side Panel. */
@JNINamespace("chrome::android")
@NullMarked
public class CustomizeChromeSidePanelHelper {
    private CustomizeChromeSidePanelHelper() {}

    @CalledByNative
    public static View createPlaceholder(WindowAndroid windowAndroid) {
        var context = windowAndroid.getContext().get();
        assert context != null;
        return new View(context);
    }
}
