// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.browser_window_helper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Stub factory for when {@link ExtensionBrowserWindowHelperBridge} isn't compiled into the build.
 *
 * <p>TODO(crbug.com/434123514): see if we can remove this stub factory.
 */
@NullMarked
public final class ExtensionBrowserWindowHelperBridgeFactory {
    private ExtensionBrowserWindowHelperBridgeFactory() {}

    @Nullable
    public static ExtensionBrowserWindowHelperBridge create() {
        return null;
    }
}
