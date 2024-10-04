// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider.AppHeaderObserver;

/**
 * An interface for the owning object to manage interaction between the bottom sheet and the rest of
 * the system.
 */
public interface ManagedBottomSheetController
        extends BottomSheetController, BottomSheetControllerProvider.Unowned, AppHeaderObserver {
    /**
     * Temporarily suppress the bottom sheet while other UI is showing. This will not itself change
     * the content displayed by the sheet.
     *
     * @param reason The reason the sheet was suppressed.
     * @return A token to unsuppress the sheet with.
     */
    int suppressSheet(@StateChangeReason int reason);

    /**
     * Unsuppress the bottom sheet. This may or may not affect the sheet depending on the state of
     * the browser (i.e. the tab switcher may be showing).
     * @param token The token that was received from suppressing the sheet.
     */
    void unsuppressSheet(int token);

    /**
     * For all contents that don't have a custom lifecycle, we remove them from show requests or
     * hide it if it is currently shown.
     */
    void clearRequestsAndHide();

    /**
     * Handle a back press event. By default this will return the bottom sheet to it's minimum /
     * peeking state if it is open. However, the sheet's content has the opportunity to intercept
     * this event and block the default behavior {@see BottomSheetContent#handleBackPress()}.
     * @return {@code true} if the sheet or content handled the back press.
     */
    boolean handleBackPress();

    /**
     * Set the hidden ratio of the browser controls.
     * @param ratio The hidden ratio of the browser controls in range [0, 1].
     */
    void setBrowserControlsHiddenRatio(float ratio);

    /** Clean up any state maintained by the controller. */
    void destroy();
}
