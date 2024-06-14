// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

/**
 * Owned and used by {@link TabStripTransitionCoordinator} to manage showing / hiding the tab strip
 * by adding a scrim in-place.
 */
// TODO (crbug.com/345849359): Move this to a new package to encapsulate strip transition code.
class ScrimTransitionHandler {
    void maybeUpdateTabStripVisibility() {
        // TODO: Trigger in-place fade transition if applicable.
    }
}
