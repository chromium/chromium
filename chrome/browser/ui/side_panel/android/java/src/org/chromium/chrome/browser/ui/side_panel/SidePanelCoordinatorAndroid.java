// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;

/** Interface for the Java counterpart of the native {@code SidePanelCoordinatorAndroid}. */
@NullMarked
public interface SidePanelCoordinatorAndroid extends ChromeAndroidTaskFeature {

    /**
     * Called when a window size change affects the side panel's visibility.
     *
     * <p>For example, when the window becomes too small, we may need to hide the side panel. When
     * the window becomes large enough again, the panel can be re-shown.
     *
     * <p>This method won't be called if a window size change doesn't affect the panel's visibility.
     *
     * @param canShowSidePanel Whether the side panel <i>can</i> be shown after a window size
     *     change. This parameter doesn't mean the panel <i>must</i> be shown or hidden. The final
     *     decision should be made by the native {@code SidePanelCoordinatorAndroid}.
     */
    void onWindowResized(boolean canShowSidePanel);
}
