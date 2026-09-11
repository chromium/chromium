// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link BottomBarView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarViewUnitTest {
    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private BottomBarView mBottomBarView;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        mBottomBarView =
                (BottomBarView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.bottom_bar_layout, null, false);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testOnTouchEvent_ConsumesTouches() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        assertTrue(
                "BottomBarView should consume touch events to prevent bleed-through",
                mBottomBarView.onTouchEvent(downEvent));
        downEvent.recycle();

        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 0, 0, 0);
        assertTrue(
                "BottomBarView should consume move events", mBottomBarView.onTouchEvent(moveEvent));
        moveEvent.recycle();

        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        assertTrue("BottomBarView should consume up events", mBottomBarView.onTouchEvent(upEvent));
        upEvent.recycle();
    }

    @Test
    public void testDispatchTouchEvent_TouchOnButton_DispatchedToChildAndConsumed() {
        layoutBottomBar();

        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);
        View newTabContainer = mBottomBarView.getContainerForAction(ActionId.NEW_TAB);
        assertNotNull(newTabButton);
        assertNotNull(newTabContainer);
        assertTrue("Button width must be > 0", newTabButton.getWidth() > 0);

        AtomicBoolean buttonReceivedDown = new AtomicBoolean(false);
        AtomicBoolean buttonReceivedUp = new AtomicBoolean(false);
        newTabButton.setOnTouchListener(
                (v, event) -> {
                    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                        buttonReceivedDown.set(true);
                    } else if (event.getActionMasked() == MotionEvent.ACTION_UP) {
                        buttonReceivedUp.set(true);
                    }
                    // ImageViews aren't clickable by default. Consume the event so the button
                    // actually claims the touch instead of falling back to
                    // BottomBarView.onTouchEvent.
                    return true;
                });

        float buttonX =
                newTabContainer.getLeft() + newTabButton.getLeft() + newTabButton.getWidth() / 2f;
        float buttonY =
                newTabContainer.getTop() + newTabButton.getTop() + newTabButton.getHeight() / 2f;

        long downTime = SystemClock.uptimeMillis();
        MotionEvent downOnButton =
                MotionEvent.obtain(
                        downTime, downTime, MotionEvent.ACTION_DOWN, buttonX, buttonY, 0);
        assertTrue(
                "Touch on button should be consumed",
                mBottomBarView.dispatchTouchEvent(downOnButton));
        assertTrue(
                "ACTION_DOWN event should be delivered to the target child button",
                buttonReceivedDown.get());
        downOnButton.recycle();

        // If the child consumed DOWN, ViewGroup routes UP to it as the active touch target.
        MotionEvent upOnButton =
                MotionEvent.obtain(
                        downTime,
                        SystemClock.uptimeMillis(),
                        MotionEvent.ACTION_UP,
                        buttonX,
                        buttonY,
                        0);
        assertTrue(
                "Up event on button should be consumed",
                mBottomBarView.dispatchTouchEvent(upOnButton));
        assertTrue(
                "ACTION_UP event should be delivered to the target child button",
                buttonReceivedUp.get());
        upOnButton.recycle();
    }

    @Test
    public void testDispatchTouchEvent_TouchOnBackground_ConsumedWithoutChildDispatch() {
        layoutBottomBar();

        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);
        View newTabContainer = mBottomBarView.getContainerForAction(ActionId.NEW_TAB);
        assertNotNull(newTabButton);
        assertNotNull(newTabContainer);
        assertTrue("Button width must be > 0", newTabButton.getWidth() > 0);
        assertTrue(
                "Button must have space to its left inside the container",
                newTabButton.getLeft() > 0);

        AtomicBoolean buttonTouched = new AtomicBoolean(false);
        newTabButton.setOnTouchListener(
                (v, event) -> {
                    buttonTouched.set(true);
                    return true;
                });

        // Tap in the container's padding to the left of the button. Since the button is missed,
        // BottomBarView should catch and consume the touch so it doesn't bleed into WebContents.
        float backgroundX = newTabContainer.getLeft() + (newTabButton.getLeft() / 2f);
        float backgroundY = newTabContainer.getTop() + (newTabContainer.getHeight() / 2f);

        long downTime = SystemClock.uptimeMillis();
        MotionEvent downOnBg =
                MotionEvent.obtain(
                        downTime, downTime, MotionEvent.ACTION_DOWN, backgroundX, backgroundY, 0);
        assertTrue(
                "Touch on background should be consumed by BottomBarView to prevent bleed-through",
                mBottomBarView.dispatchTouchEvent(downOnBg));
        assertFalse(
                "Touch on background should NOT be delivered to the child button",
                buttonTouched.get());
        downOnBg.recycle();

        MotionEvent upOnBg =
                MotionEvent.obtain(
                        downTime,
                        SystemClock.uptimeMillis(),
                        MotionEvent.ACTION_UP,
                        backgroundX,
                        backgroundY,
                        0);
        assertTrue(
                "Up event on background should be consumed by BottomBarView",
                mBottomBarView.dispatchTouchEvent(upOnBg));
        assertFalse(
                "Up event on background should NOT be delivered to the child button",
                buttonTouched.get());
        upOnBg.recycle();
    }

    @Test
    public void testDispatchTouchEvent_OnGoneButton_ConsumedByBar() {
        layoutBottomBar();

        View newTabButton = mBottomBarView.findViewById(R.id.new_tab_button);
        View newTabContainer = mBottomBarView.getContainerForAction(ActionId.NEW_TAB);
        assertNotNull(newTabButton);
        assertNotNull(newTabContainer);

        float buttonX =
                newTabContainer.getLeft() + newTabButton.getLeft() + newTabButton.getWidth() / 2f;
        float buttonY =
                newTabContainer.getTop() + newTabButton.getTop() + newTabButton.getHeight() / 2f;

        // Make sure GONE buttons don't leave touch holes in the bar.
        mBottomBarView.setButtonVisibility(ActionId.NEW_TAB, false);
        assertEquals(View.GONE, newTabContainer.getVisibility());

        AtomicBoolean buttonTouched = new AtomicBoolean(false);
        newTabButton.setOnTouchListener(
                (v, event) -> {
                    buttonTouched.set(true);
                    return true;
                });

        long downTime = SystemClock.uptimeMillis();
        MotionEvent downOnGone =
                MotionEvent.obtain(
                        downTime, downTime, MotionEvent.ACTION_DOWN, buttonX, buttonY, 0);
        assertTrue(
                "Touch on GONE button coordinates should be consumed by BottomBarView",
                mBottomBarView.dispatchTouchEvent(downOnGone));
        assertFalse(
                "Touch on GONE button coordinates should NOT be delivered to the child button",
                buttonTouched.get());
        downOnGone.recycle();

        MotionEvent upOnGone =
                MotionEvent.obtain(
                        downTime,
                        SystemClock.uptimeMillis(),
                        MotionEvent.ACTION_UP,
                        buttonX,
                        buttonY,
                        0);
        assertTrue(
                "Up event on GONE button coordinates should be consumed by BottomBarView",
                mBottomBarView.dispatchTouchEvent(upOnGone));
        assertFalse(
                "Touch on GONE button coordinates should NOT be delivered to the child button",
                buttonTouched.get());
        upOnGone.recycle();
    }

    private void layoutBottomBar() {
        mBottomBarView.setButtonVisibility(ActionId.NEW_TAB, true);
        mBottomBarView.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        mBottomBarView.layout(0, 0, 1000, 100);
    }
}
