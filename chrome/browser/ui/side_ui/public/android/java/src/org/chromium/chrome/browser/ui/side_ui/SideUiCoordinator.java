// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/**
 * Coordinator for "side UI," with "side UI" referring to views that will anchor to either the left
 * or right side of the main browser window.
 */
@NullMarked
public interface SideUiCoordinator {

    /**
     * The side of the window ({@link #START} or {@link #END}) that a {@link SideUiContainer} will
     * anchor to.
     */
    @IntDef({AnchorSide.START, AnchorSide.END})
    @interface AnchorSide {
        int START = 0;
        int END = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * POD-type that holds the info for a request to reposition or resize a {@link SideUiContainer}.
     */
    final class SideUiContainerProperties {
        final @AnchorSide int mAnchorSide;
        final @Px int mWidth;

        public SideUiContainerProperties(@AnchorSide int anchorSide, @Px int width) {
            mAnchorSide = anchorSide;
            mWidth = width;
        }
    }

    /**
     * Registers a {@link SideUiContainer} to be maintained by this coordinator.
     *
     * @param sideUiContainer The {@link SideUiContainer} to register.
     */
    void registerSideUiContainer(SideUiContainer sideUiContainer);

    /**
     * Unregisters a {@link SideUiContainer} such that it will no longer be maintained by this
     * coordinator.
     *
     * @param sideUiContainer The {@link SideUiContainer} to unregister.
     */
    void unregisterSideUiContainer(SideUiContainer sideUiContainer);

    /**
     * Requests that the registered {@link SideUiContainer} change its width.
     * <strong>Important:</strong> this should only be called by the feature that owns the affected
     * {@link SideUiContainer}.
     *
     * @param properties The {@link SideUiContainerProperties} that defines the new requested
     *     position for the registered {@link SideUiContainer}.
     */
    void requestUpdateContainer(SideUiContainerProperties properties);

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
