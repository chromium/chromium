// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.side_panel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating an {@link ExtensionSidePanelManagerBridge}. */
@NullMarked
public final class ExtensionSidePanelManagerBridgeFactory {
    private ExtensionSidePanelManagerBridgeFactory() {}

    // Mark as nullable to be consistent with the stub factory in
    // //chrome/browser/ui/android/extensions/side_panel/stub.
    @Nullable
    public static ExtensionSidePanelManagerBridge create() {
        return new ExtensionSidePanelManagerBridgeImpl();
    }
}
