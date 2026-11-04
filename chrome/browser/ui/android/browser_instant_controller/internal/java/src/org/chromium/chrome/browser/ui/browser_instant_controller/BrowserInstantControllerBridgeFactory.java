// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_instant_controller;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating a {@link BrowserInstantControllerBridge}. */
@NullMarked
public final class BrowserInstantControllerBridgeFactory {
    private BrowserInstantControllerBridgeFactory() {}

    // Mark as nullable to be consistent with the stub factory in
    // //chrome/browser/ui/android/browser_instant_controller/stub.
    @Nullable
    public static BrowserInstantControllerBridge create() {
        return new BrowserInstantControllerBridgeImpl();
    }
}
