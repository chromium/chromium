// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.browser.ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE;

import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Junit4 rule for tests testing the bottom sheet. This rule creates a new, separate bottom sheet
 * to test with.
 */
@CommandLineFlags.Add({DISABLE_FIRST_RUN_EXPERIENCE})
public class BottomSheetTestRule extends ChromeTabbedActivityTestRule {
    /** An observer used to record events that occur with respect to the bottom sheet. */
    public static class Observer extends EmptyBottomSheetObserver {
        /** A {@link CallbackHelper} that can wait for the bottom sheet to be closed. */
        public final CallbackHelper mClosedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the bottom sheet to be opened. */
        public final CallbackHelper mOpenedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onOffsetChanged event. */
        public final CallbackHelper mOffsetChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onSheetContentChanged event. */
        public final CallbackHelper mContentChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be in its full state. */
        public final CallbackHelper mFullCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be hidden. */
        public final CallbackHelper mHiddenCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to load a URL. */
        public final CallbackHelper mLoadUrlCallbackHelper = new CallbackHelper();

        /** The last value that the onOffsetChanged event sent. */
        private float mLastOffsetChangedValue;

        @Override
        public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
            mLastOffsetChangedValue = heightFraction;
            mOffsetChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOpened(@StateChangeReason int reason) {
            mOpenedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            mClosedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetContentChanged(BottomSheetContent newContent) {
            mContentChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetStateChanged(int newState) {
            if (newState == BottomSheetController.SheetState.HIDDEN) {
                mHiddenCallbackHelper.notifyCalled();
            } else if (newState == BottomSheetController.SheetState.FULL) {
                mFullCallbackHelper.notifyCalled();
            }
        }

        /** @return The last value passed in to {@link #onSheetOffsetChanged(float)}. */
        public float getLastOffsetChangedValue() {
            return mLastOffsetChangedValue;
        }

        @Override
        public void onLoadUrl(String url) {
            mLoadUrlCallbackHelper.notifyCalled();
        }
    }

    /** A handle to the controller for the sheet. */
    private BottomSheetController mSheetController;

    /** A handle to the sheet's observer. */
    private Observer mObserver;

    protected void afterStartingActivity() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSheetController = getActivity().getBottomSheetController(); });

        mObserver = new Observer();
        mSheetController.addObserver(mObserver);
    }

    // TODO (aberent): The Chrome test rules currently bypass ActivityTestRule.launchActivity, hence
    // don't call beforeActivityLaunched and afterActivityLaunched as defined in the
    // ActivityTestRule interface. To work round this override the methods that start activities.
    // See https://crbug.com/726444.
    @Override
    public void startMainActivityOnBlankPage() {
        super.startMainActivityOnBlankPage();
        afterStartingActivity();
    }

    public Observer getObserver() {
        return mObserver;
    }

    public BottomSheetController getBottomSheetController() {
        return mSheetController;
    }

    public BottomSheet getBottomSheet() {
        return (BottomSheet) mSheetController.getBottomSheetViewForTesting();
    }

    /**
     * Set the bottom sheet's state on the UI thread.
     *
     * @param state   The state to set the sheet to.
     * @param animate If the sheet should animate to the provided state.
     */
    public void setSheetState(int state, boolean animate) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getBottomSheetController().setSheetStateForTesting(state, animate));
    }

    /**
     * Set the bottom sheet's offset from the bottom of the screen on the UI thread.
     *
     * @param offset The offset from the bottom that the sheet should be.
     */
    public void setSheetOffsetFromBottom(float offset) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSheetController.setSheetOffsetFromBottomForTesting(offset));
    }

    public BottomSheetContent getBottomSheetContent() {
        return mSheetController.getCurrentSheetContent();
    }

    /**
     * Wait for an update to start and finish.
     */
    public static void waitForWindowUpdates() {
        final long maxWindowUpdateTimeMs = scaleTimeout(1000);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.waitForWindowUpdate(null, maxWindowUpdateTimeMs);
        device.waitForIdle(maxWindowUpdateTimeMs);
    }
}
