// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.view.MotionEvent;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/** Utilities to support testing with the {@link BottomSheetController}. */
public class BottomSheetTestSupport {
    /** A handle to the actual implementation class of the {@link BottomSheetController}. */
    BottomSheetControllerImpl mController;

    /** @param controller A handle to the public {@link BottomSheetController}. */
    public BottomSheetTestSupport(BottomSheetController controller) {
        mController = (BottomSheetControllerImpl) controller;
    }

    /**
     * Sets whether the screen should be considered small for testing.
     *
     * @param isSmallScreen Whether the screen should be considered small for testing.
     */
    public static void setSmallScreen(boolean isSmallScreen) {
        BottomSheetCoordinator.setSmallScreenForTesting(isSmallScreen);
    }

    /**
     * See {@link ManagedBottomSheetController#suppressSheet(int)}.
     *
     * @param reason The reason the bottom sheet is being suppressed.
     */
    public int suppressSheet(@StateChangeReason int reason) {
        return mController.suppressSheet(reason);
    }

    /**
     * See {@link ManagedBottomSheetController#unsuppressSheet(int)}.
     *
     * @param token The token used to suppress the bottom sheet.
     */
    public void unsuppressSheet(int token) {
        mController.unsuppressSheet(token);
    }

    /** See {@link ManagedBottomSheetController#handleBackPress()}. */
    public boolean handleBackPress() {
        return mController.handleBackPress();
    }

    /** End all animations on the sheet for testing purposes. */
    public void endAllAnimations() {
        if (getBottomSheet() != null) mController.endAnimationsForTesting();
    }

    /**
     * See {@link BottomSheetCoordinator#setSheetOffsetFromBottom(float, int)}.
     *
     * @param offset The offset from the bottom in pixels.
     * @param reason The reason the sheet offset is being set.
     */
    public void setSheetOffsetFromBottom(float offset, @StateChangeReason int reason) {
        getBottomSheet().setSheetOffsetFromBottom(offset, reason);
    }

    public void setBottomMargin(int offset) {
        getBottomSheet().setBottomMargin(offset);
    }

    /** See {@link BottomSheetCoordinator#getMaxOffsetPx()}. */
    public float getMaxOffsetPx() {
        return getBottomSheet().getMaxOffsetPx();
    }

    /** See {@link BottomSheetCoordinator#getCurrentOffsetPx()}. */
    public float getCurrentOffsetPx() {
        return getBottomSheet().getCurrentOffsetPx();
    }

    public float getOffsetFromBrowserControls() {
        return getBottomSheet().getOffsetFromBrowserControls();
    }

    /** See {@link BottomSheetCoordinator#getFullRatio()}. */
    public float getFullRatio() {
        return getBottomSheet().getFullRatio();
    }

    /** See {@link BottomSheetCoordinator#getHiddenRatio()}. */
    public float getHiddenRatio() {
        return getBottomSheet().getHiddenRatio();
    }

    /** See {@link BottomSheetCoordinator#getOpeningState()}. */
    @SheetState
    public int getOpeningState() {
        return getBottomSheet().getOpeningState();
    }

    /**
     * See {@link BottomSheetCoordinator#showContent(BottomSheetContent)}.
     *
     * @param content The content to show in the bottom sheet.
     */
    public void showContent(BottomSheetContent content) {
        getBottomSheet().showContent(content);
    }

    /**
     * See {@link BottomSheetCoordinator#shouldGestureMoveSheet()}.
     *
     * @param initialEvent The initial motion event of the gesture.
     * @param currentEvent The current motion event of the gesture.
     */
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
     * Force triggering `onSheetStateChanged` events.
     *
     * @param state The state the sheet should be in.
     */
    public void setInternalCurrentState(@SheetState int state) {
        getBottomSheet().setInternalCurrentState(state, StateChangeReason.NONE);
    }

    /**
     * WARNING: This destroys the internal sheet state. Only use in tests and only use once!
     *
     * <p>To simulate scrolling, this method puts the sheet in a permanent scrolling state.
     *
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

    /** Returns the bottom sheet coordinator. */
    private BottomSheetCoordinator getBottomSheet() {
        return mController.getBottomSheetForTesting();
    }

    /** Returns the container for the bottom sheet. */
    public ViewGroup getSheetContainer() {
        return mController.getBottomSheetContainerForTesting();
    }

    /** Returns whether has any token to suppress the bottom sheet. */
    public boolean hasSuppressionTokens() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mController.hasSuppressionTokensForTesting());
    }

    public void setEdgeToEdgeBottomInsetSupplier(Supplier<Integer> edgeToEdgeBottomInsetSupplier) {
        getBottomSheet().setEdgeToEdgeBottomInsetSupplierForTesting(edgeToEdgeBottomInsetSupplier);
    }

    /**
     * Wait for the bottom sheet to enter the specified state. If the sheet is already in the
     * specified state, this method returns immediately.
     *
     * @param controller The controller for the bottom sheet.
     * @param state The state to wait for.
     */
    public static void waitForState(BottomSheetController controller, @SheetState int state) {
        CallbackHelper stateChangeHelper = new CallbackHelper();
        final BottomSheetObserver observer =
                new BottomSheetObserver() {
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
                new BottomSheetObserver() {
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
                new BottomSheetObserver() {
                    @Override
                    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
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
