// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.browser_window_helper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating an {@link ExtensionBrowserWindowHelperBridge}. */
@NullMarked
public final class ExtensionBrowserWindowHelperBridgeFactory {
    private ExtensionBrowserWindowHelperBridgeFactory() {}

    // Mark as nullable to be consistent with the stub factory in
    // //chrome/browser/ui/android/extensions/browser_window_helper/stub.
    @Nullable
    public static ExtensionBrowserWindowHelperBridge create() {
        return new ExtensionBrowserWindowHelperBridgeImpl();
    }
}
