// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.windowing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

/**
 * Stub factory for when {@link ExtensionWindowControllerBridge} isn't compiled into the build.
 *
 * <p>TODO(crbug.com/434123514): see if we can remove this stub factory.
 */
@NullMarked
public final class ExtensionWindowControllerBridgeFactory {
    private ExtensionWindowControllerBridgeFactory() {}

    @Nullable
    public static ExtensionWindowControllerBridge create(ChromeAndroidTask chromeAndroidTask) {
        return null;
    }
}
