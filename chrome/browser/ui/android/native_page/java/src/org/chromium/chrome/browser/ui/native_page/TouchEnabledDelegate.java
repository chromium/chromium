// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

/**
 * Delegate used by the {@link ContextMenuManager} to disable touch events on the outer view
 * while the context menu is open.
 */
public interface TouchEnabledDelegate {
    void setTouchEnabled(boolean enabled);
}
