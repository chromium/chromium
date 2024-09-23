// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.view.MotionEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

import java.util.concurrent.TimeoutException;

/** Utilities to support testing with the {@link BottomSheetController}. */
public class BottomSheetTestSupport {
    /** A handle to the actual implementation class of the {@link BottomSheetController}. */
    BottomSheetControllerImpl mController;

    /** @param controller A handle to the public {@link BottomSheetController}. */
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

    /** @see {@link BottomSheet#shouldGestureMoveSheet()} */
    public boolean shouldGestureMoveSheet(MotionEvent initialEvent, MotionEvent currentEvent) {
        return getBottomSheet().shouldGestureMoveSheet(initialEvent, currentEvent);
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

    public void forceClickOutsideTheSheet() {
        getBottomSheet().setSheetState(SheetState.HIDDEN, false, StateChangeReason.TAP_SCRIM);
    }

    /** @return The bottom sheet view. */
    private BottomSheet getBottomSheet() {
        return (BottomSheet) mController.getBottomSheetViewForTesting();
    }

    /**
     * @return Whether has any token to suppress the bottom sheet.
     */
    public boolean hasSuppressionTokens() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mController.hasSuppressionTokensForTesting());
    }

    /**
     * Wait for the bottom sheet to enter the specified state. If the sheet is already in the
     * specified state, this method returns immediately.
     * @param controller The controller for the bottom sheet.
     * @param state The state to wait for.
     */
    public static void waitForState(BottomSheetController controller, @SheetState int state) {
        CallbackHelper stateChangeHelper = new CallbackHelper();
        final BottomSheetObserver observer =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        if (state == newState) stateChangeHelper.notifyCalled();
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (controller.getSheetState() == state) {
                        stateChangeHelper.notifyCalled();
                    } else {
                        controller.addObserver(observer);
                    }
                });

        try {
            stateChangeHelper.waitForOnly();
        } catch (TimeoutException ex) {
            assert false : "Bottom sheet state never changed to " + sheetStateToString(state);
        }

        ThreadUtils.runOnUiThreadBlocking(() -> controller.removeObserver(observer));
    }

    /**
     * Wait for the bottom sheet to enter the half or full state. If the sheet is already in either
     * state, this method returns immediately.
     *
     * @param controller The controller for the bottom sheet.
     */
    public static void waitForOpen(BottomSheetController controller) {
        CallbackHelper stateChangeHelper = new CallbackHelper();

        final BottomSheetObserver observer =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        if (newState == BottomSheetController.SheetState.HALF
                                || newState == SheetState.FULL) {
                            stateChangeHelper.notifyCalled();
                        }
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (controller.getSheetState() == BottomSheetController.SheetState.HALF
                            || controller.getSheetState()
                                    == BottomSheetController.SheetState.FULL) {
                        stateChangeHelper.notifyCalled();
                    } else {
                        controller.addObserver(observer);
                    }
                });

        try {
            stateChangeHelper.waitForOnly();
        } catch (TimeoutException ex) {
            assert false
                    : "Bottom sheet state never half or full. Current State: "
                            + sheetStateToString(controller.getSheetState());
        }

        ThreadUtils.runOnUiThreadBlocking(() -> controller.removeObserver(observer));
    }

    /**
     * Wait for the specified content to be shown. If the content is already showing this method
     * returns immediately. If the sheet is suppressed when this method is called, the expected
     * content change is to null.
     *
     * @param controller The controller for the bottom sheet.
     * @param content The content to wait for.
     */
    public static void waitForContentChange(
            BottomSheetController controller, BottomSheetContent content) {
        BottomSheetControllerImpl controllerImpl = (BottomSheetControllerImpl) controller;
        boolean contentShouldBeNull = controllerImpl.hasSuppressionTokensForTesting();

        if ((contentShouldBeNull && controller.getCurrentSheetContent() == null)
                || controller.getCurrentSheetContent() == content) {
            return;
        }

        CallbackHelper contentChangeHelper = new CallbackHelper();
        BottomSheetObserver observer =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetContentChanged(BottomSheetContent newContent) {
                        if ((contentShouldBeNull && newContent == null) || content == newContent) {
                            contentChangeHelper.notifyCalled();
                        }
                    }
                };
        controller.addObserver(observer);
        try {
            contentChangeHelper.waitForOnly();
        } catch (TimeoutException ex) {
            assert false : "Bottom sheet content never changed!";
        }
        controller.removeObserver(observer);
    }

    /**
     * @param state The state of the bottom sheet to convert to a string.
     * @return The string version of the sheet state.
     */
    private static String sheetStateToString(@SheetState int state) {
        switch (state) {
            case SheetState.HIDDEN:
                return "HIDDEN";
            case SheetState.PEEK:
                return "PEEK";
            case SheetState.HALF:
                return "HALF";
            case SheetState.FULL:
                return "FULL";
            case SheetState.SCROLLING:
                return "SCROLLING";
            default:
                break;
        }
        return "UNKNOWN STATE";
    }
}
