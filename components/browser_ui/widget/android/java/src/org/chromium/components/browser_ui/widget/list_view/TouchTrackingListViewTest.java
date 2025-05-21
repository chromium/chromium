// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.list_view;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TouchTrackingListViewTest {

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Test
    @SmallTest
    public void onInterceptTouchEvent_gestureCompleted_recordTouchInfo() {
        TouchTrackingListView touchTrackingListView = setUpTouchTrackingListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Point centerPoint = getCenterPoint(touchTrackingListView);
                    long motionDownTime = SystemClock.uptimeMillis();

                    // Act:
                    // Use a sequence of "ACTION_DOWN + ACTION_UP" events to simulate a
                    // "single tap up" gesture.
                    testOnInterceptTouchEvent(
                            touchTrackingListView,
                            MotionEventTestUtils.createMouseMotionEvent(
                                    motionDownTime,
                                    /* eventTime= */ motionDownTime,
                                    MotionEvent.ACTION_DOWN,
                                    centerPoint.x,
                                    centerPoint.y));
                    testOnInterceptTouchEvent(
                            touchTrackingListView,
                            MotionEventTestUtils.createMouseMotionEvent(
                                    motionDownTime,
                                    /* eventTime= */ motionDownTime + 50,
                                    MotionEvent.ACTION_UP,
                                    centerPoint.x,
                                    centerPoint.y));

                    // Assert
                    MotionEventInfo touchInfo = touchTrackingListView.getLastSingleTapUp();
                    assertNotNull(touchInfo);
                    assertEquals(MotionEvent.ACTION_UP, touchInfo.action);
                    assertEquals(InputDevice.SOURCE_MOUSE, touchInfo.source);
                    assertEquals(1, touchInfo.toolType.length);
                    assertEquals(MotionEvent.TOOL_TYPE_MOUSE, touchInfo.toolType[0]);
                });
    }

    @Test
    @SmallTest
    public void onInterceptTouchEvent_gestureNotCompleted_doNotRecordTouchInfo() {
        TouchTrackingListView touchTrackingListView = setUpTouchTrackingListView();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Point centerPoint = getCenterPoint(touchTrackingListView);
                    long motionDownTime = SystemClock.uptimeMillis();

                    // Act:
                    // A single ACTION_DOWN event shouldn't complete any gesture.
                    testOnInterceptTouchEvent(
                            touchTrackingListView,
                            MotionEventTestUtils.createMouseMotionEvent(
                                    motionDownTime,
                                    /* eventTime= */ motionDownTime,
                                    MotionEvent.ACTION_DOWN,
                                    centerPoint.x,
                                    centerPoint.y));

                    // Assert
                    assertNull(touchTrackingListView.getLastSingleTapUp());
                });
    }

    private TouchTrackingListView setUpTouchTrackingListView() {
        BlankUiTestActivity.setTestLayout(R.layout.touch_tracking_list_view_test);
        Activity testActivity = mActivityRule.launchActivity(/* startIntent= */ null);
        TouchTrackingListView touchTrackingListView =
                testActivity.findViewById(R.id.touch_tracking_list_view_for_testing);

        // ListView#setAdapter() will schedule a layout pass of the view tree, so call it on the
        // main thread to avoid flaky tests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ArrayAdapter<String> adapter =
                            new ArrayAdapter<>(
                                    touchTrackingListView.getContext(),
                                    R.layout.touch_tracking_list_view_test_item,
                                    R.id.touch_tracking_list_view_test_item_view,
                                    new String[] {"test item"});
                    touchTrackingListView.setAdapter(adapter);
                    touchTrackingListView.setOnItemClickListener(
                            (parent, view, position, id) -> {
                                // Nothing to do for now, but setting an OnItemClickListener
                                // helps test fidelity since
                                // (1) we will test touch events, and
                                // (2) in production a TouchTrackingListView usually has an
                                //     OnItemClickListener.
                            });
                });

        return touchTrackingListView;
    }

    private static Point getCenterPoint(View view) {
        Rect visibleRect = new Rect();
        view.getLocalVisibleRect(visibleRect);

        Point centerPoint = new Point(visibleRect.centerX(), visibleRect.centerY());
        assertTrue(
                "The center point shouldn't be (0, 0). Is the View set up correctly and visible?",
                centerPoint.x != 0 && centerPoint.y != 0);

        return centerPoint;
    }

    /**
     * Tests {@link TouchTrackingListView#onInterceptTouchEvent(MotionEvent)}.
     *
     * <p>Note that we directly invoke {@code onInterceptTouchEvent()} instead of using {@link
     * ViewGroup#dispatchTouchEvent(MotionEvent)}.
     *
     * <p>This is because we observed that {@code dispatchTouchEvent()} doesn't trigger {@code
     * onInterceptTouchEvent()} in the same way as when {@link TouchTrackingListView} receives real
     * touch events. In particular, the following happens when simulating a click by calling {@code
     * dispatchTouchEvent()} on {@link TouchTrackingListView}:
     *
     * <ol>
     *   <li>{@code dispatchTouchEvent(ACTION_DOWN)}
     *   <li>{@code onInterceptTouchEvent(ACTION_DOWN)}
     *   <li>{@code onTouchEvent(ACTION_DOWN)}
     *   <li>{@code dispatchTouchEvent(ACTION_UP)}
     *   <li>{@code onTouchEvent(ACTION_UP)}; Here {@code onInterceptTouchEvent(ACTION_UP)} wasn't
     *       triggered, which is different from the behavior on a real device.
     * </ol>
     *
     * @param touchTrackingListView the {@link TouchTrackingListView} to test.
     * @param motionEvent the {@link MotionEvent} to be processed by the {@link
     *     TouchTrackingListView}.
     */
    private static void testOnInterceptTouchEvent(
            TouchTrackingListView touchTrackingListView, MotionEvent motionEvent) {
        boolean eventIntercepted = touchTrackingListView.onInterceptTouchEvent(motionEvent);
        assertFalse("TouchTrackingListView should never intercept touch events.", eventIntercepted);
    }
}
