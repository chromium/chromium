// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

/** Factory for creating an {@link ExtensionWindowControllerBridge}. */
@NullMarked
public final class ExtensionWindowControllerBridgeFactory {
    private ExtensionWindowControllerBridgeFactory() {}

    /**
     * Creates an {@link ExtensionWindowControllerBridge} for the given {@link ChromeAndroidTask}.
     *
     * <p>Note: this class is compiled using the {@code android_library_factory} GN template, so
     * this method will return null if {@link ExtensionWindowControllerBridgeImpl} isn't compiled
     * into the build.
     */
    @Nullable
    public static ExtensionWindowControllerBridge create(ChromeAndroidTask chromeAndroidTask) {
        return new ExtensionWindowControllerBridgeImpl(chromeAndroidTask);
    }
}
