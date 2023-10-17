// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import static org.mockito.ArgumentMatchers.anyInt;

import android.view.MotionEvent;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;

import java.util.ArrayList;
import java.util.List;

/** The Unittest of {@link SwipeGestureListener}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SwipeGestureListenerTest {
    private SwipeGestureListener mListener;

    @Mock private SwipeHandler mHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mListener = new SwipeGestureListener(null, mHandler, 1, 1);
    }

    @Test
    @SmallTest
    public void testOnSwipeLeftDirection() {
        testSwipeByGivenDirection(
                ScrollDirection.LEFT, buildEventStream(100.0f, 100.0f, -5.0f, -3.0f, 10));
    }

    @Test
    @SmallTest
    public void testOnSwipeRightDirection() {
        testSwipeByGivenDirection(
                ScrollDirection.RIGHT, buildEventStream(100.0f, 100.0f, 5.0f, -3.0f, 10));
    }

    @Test
    @SmallTest
    public void testOnSwipeUpDirection() {
        testSwipeByGivenDirection(
                ScrollDirection.UP, buildEventStream(100.0f, 100.0f, 3.0f, -5.0f, 10));
    }

    @Test
    @SmallTest
    public void testOnSwipeDownDirection() {
        testSwipeByGivenDirection(
                ScrollDirection.DOWN, buildEventStream(100.0f, 100.0f, 2.0f, 5.0f, 10));
    }

    private void testSwipeByGivenDirection(int expectedDirection, List<MotionEvent> eventStream) {
        Mockito.when(mHandler.isSwipeEnabled(anyInt())).thenReturn(true);
        for (MotionEvent event : eventStream) {
            mListener.onTouchEvent(event);
        }
        ArgumentCaptor<MotionEvent> argumentCaptor = ArgumentCaptor.forClass(MotionEvent.class);
        Mockito.verify(mHandler)
                .onSwipeStarted(Mockito.eq(expectedDirection), argumentCaptor.capture());
        boolean found = false;
        for (MotionEvent event : eventStream) {
            if (Math.abs(event.getRawX() - argumentCaptor.getValue().getRawX()) < 0.1) {
                found = true;
                break;
            }
        }
        Assert.assertTrue("Can not found the expected first move event", found);
    }

    private List<MotionEvent> buildEventStream(
            float startX, float startY, float offsetX, float offSetY, int count) {
        List<MotionEvent> list = new ArrayList<>();
        list.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, startX, startY, 0));
        for (int i = 1; i < count - 1; i++) {
            list.add(
                    MotionEvent.obtain(
                            0,
                            0,
                            MotionEvent.ACTION_MOVE,
                            startX + i * offsetX,
                            startY + i * offSetY,
                            0));
        }
        list.add(
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_UP,
                        startX + (count - 1) * offsetX,
                        startY + (count - 1) * offSetY,
                        0));
        return list;
    }
}
