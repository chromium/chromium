// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test_support;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.chrome.browser.toolbar.ToolbarManager;

/** Test support for injecting test behaviour from C++ tests into Java ToolbarManger. */
@JNINamespace("toolbar_manager")
public class ToolbarManagerTestHelper {
    /**
     * Sets whether to skip recreating the activity when the settings are changed. It should only
     * be true in testing.
     */
    @CalledByNative
    public static void setSkipRecreateForTesting(boolean skipRecreating) {
        ToolbarManager.setSkipRecreateActivityWhenStartSurfaceEnabledStateChangesForTesting(
                skipRecreating);
    }
}
