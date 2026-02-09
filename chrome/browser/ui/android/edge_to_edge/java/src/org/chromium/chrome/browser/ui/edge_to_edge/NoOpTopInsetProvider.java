// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import org.chromium.build.annotations.NullMarked;

/**
 * A no-op implementation of {@link TopInsetProvider} that provides placeholder functionality for
 * scenarios where a real top inset implementation is not needed, but is required to satisfy
 * compilation.
 */
@NullMarked
public class NoOpTopInsetProvider implements TopInsetProvider {

    @Override
    public void addObserver(Observer observer) {
        // No-op: This implementation doesn't track observers.
    }

    @Override
    public void removeObserver(Observer observer) {
        // No-op: This implementation doesn't track observers.
    }

    @Override
    public void destroy() {
        // No-op: No resources to clean up.
    }
}
