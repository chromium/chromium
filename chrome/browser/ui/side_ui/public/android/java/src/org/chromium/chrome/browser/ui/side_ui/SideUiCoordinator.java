// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.build.annotations.NullMarked;

/**
 * Coordinator for "side UI," with "side UI" referring to views that will anchor to either the left
 * or right side of the main browser window.
 */
@NullMarked
public interface SideUiCoordinator {

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
