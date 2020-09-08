// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/** Utilities to support testing with the {@link BottomSheetController}. */
public class BottomSheetTestSupport {
    /** A handle to the actual implementation class of the {@link BottomSheetController}. */
    BottomSheetControllerImpl mController;

    /**
     * @param controller A handle to the public {@link BottomSheetController}.
     */
    public BottomSheetTestSupport(BottomSheetController controller) {
        mController = (BottomSheetControllerImpl) controller;
    }

    /** @param isSmallScreen Whether the screen should be considered small for testing. */
    public static void setSmallScreen(boolean isSmallScreen) {
        BottomSheet.setSmallScreenForTesting(isSmallScreen);
    }

    /** @see {@link ManagedBottomSheetController#suppressSheet(int)} */
    public int suppressSheet(@StateChangeReason int reason) {
        return mController.suppressSheet(reason);
    }

    /** @see {@link ManagedBottomSheetController#unsuppressSheet(int)} */
    public void unsuppressSheet(int token) {
        mController.unsuppressSheet(token);
    }

    /** @see {@link ManagedBottomSheetController#handleBackPress()} */
    public boolean handleBackPress() {
        return mController.handleBackPress();
    }

    /** End all animations on the sheet for testing purposes. */
    public void endAllAnimations() {
        if (getBottomSheet() != null) mController.endAnimationsForTesting();
    }

    /** @see {@link BottomSheet#setSheetOffsetFromBottom(float, int)} */
    public void setSheetOffsetFromBottom(float offset, @StateChangeReason int reason) {
        getBottomSheet().setSheetOffsetFromBottom(offset, reason);
    }

    /** @see {@link BottomSheet#getFullRatio()} */
    public float getFullRatio() {
        return getBottomSheet().getFullRatio();
    }

    /** @see {@link BottomSheet#getHiddenRatio()} */
    public float getHiddenRatio() {
        return getBottomSheet().getHiddenRatio();
    }

    /** @see {@link BottomSheet#getOpeningState()} */
    @SheetState
    public int getOpeningState() {
        return getBottomSheet().getOpeningState();
    }

    /** @see {@link BottomSheet#showContent(BottomSheetContent)}} */
    public void showContent(BottomSheetContent content) {
        getBottomSheet().showContent(content);
    }

    /**
     * TODO(mdjones): Remove this method and others that can be accessed via the
     *                {@link BottomSheetController}.
     * @see {@link BottomSheet#getToolbarShadowHeight()}
     */
    public int getToolbarShadowHeight() {
        return getBottomSheet().getToolbarShadowHeight();
    }

    /**
     * Force the sheet's state for testing.
     * @param state The state the sheet should be in.
     * @param animate Whether the sheet should animate to the specified state.
     */
    public void setSheetState(@SheetState int state, boolean animate) {
        mController.setSheetStateForTesting(state, animate);
    }

    /**
     * WARNING: This destroys the internal sheet state. Only use in tests and only use once!
     *
     * To simulate scrolling, this method puts the sheet in a permanent scrolling state.
     * @return The target state of the bottom sheet (to check thresholds).
     */
    @SheetState
    public int forceScrolling(float sheetHeight, float yVelocity) {
        return getBottomSheet().forceScrollingStateForTesting(sheetHeight, yVelocity);
    }

    /** Dismiss all content currently queued in the controller including custom lifecycles. */
    public void forceDismissAllContent() {
        mController.forceDismissAllContent();
    }

    /** @return The bottom sheet view. */
    private BottomSheet getBottomSheet() {
        return (BottomSheet) mController.getBottomSheetViewForTesting();
    }
}
