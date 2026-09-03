// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.side_panel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Stub factory for when {@link ExtensionSidePanelManagerBridge} isn't compiled into the build. */
@NullMarked
public final class ExtensionSidePanelManagerBridgeFactory {
    private ExtensionSidePanelManagerBridgeFactory() {}

    @Nullable
    public static ExtensionSidePanelManagerBridge create() {
        return null;
    }
}
