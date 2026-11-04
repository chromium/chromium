// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_instant_controller;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Stub factory for when {@link BrowserInstantControllerBridge} isn't compiled into the build (i.e.
 * when the WebUI NTP isn't enabled).
 */
@NullMarked
public final class BrowserInstantControllerBridgeFactory {
    private BrowserInstantControllerBridgeFactory() {}

    @Nullable
    public static BrowserInstantControllerBridge create() {
        return null;
    }
}
