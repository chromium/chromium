// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;

/** Interface representing the bottom bar. */
@NullMarked
public interface BottomBar {
    /** Returns the view representing the bottom bar. */
    View getView();

    /**
     * Informs the bottom bar that its parent has changed.
     *
     * @param host The new host of the bottom bar.
     */
    void setParent(@Host int host);
}
