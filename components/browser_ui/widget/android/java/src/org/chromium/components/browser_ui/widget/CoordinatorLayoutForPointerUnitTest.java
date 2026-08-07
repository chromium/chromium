// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.content.Context;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link CoordinatorLayoutForPointer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CoordinatorLayoutForPointerUnitTest {

    private Activity mActivity;
    private CoordinatorLayoutForPointer mCoordinator;
    private MockView mChildView;
    private PointerIcon mTestIcon;
    private PointerIcon mCrosshairIcon;

    static class MockView extends View {
        MotionEvent mReceivedEvent;
        int mReceivedPointerIndex;
        PointerIcon mReturnIcon;

        public MockView(Context context) {
            super(context);
        }

        @Override
        public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
            mReceivedEvent = MotionEvent.obtain(event);
            mReceivedPointerIndex = pointerIndex;
            return mReturnIcon;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mCoordinator = new CoordinatorLayoutForPointer(mActivity, null);
        mChildView = new MockView(mActivity);
        mTestIcon = PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_TEXT);
        mCrosshairIcon = PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_CROSSHAIR);
        mChildView.mReturnIcon = mTestIcon;

        mCoordinator.addView(mChildView);
        // Layout the coordinator so it has non-zero size, allowing it to resolve
        // pointer icons for coordinates within these bounds.
        mCoordinator.layout(0, 0, 1000, 1000);
    }

    @Test
    public void testOnResolvePointerIcon_offsetsCoordinates() {
        // Position the child at (10, 20) with size (100, 100).
        mChildView.layout(10, 10, 110, 110);
        mChildView.setVisibility(View.VISIBLE);

        // Create a hover event at (50, 60) which is inside the child bounds.
        MotionEvent event = MotionEvent.obtain(
                0, 0, MotionEvent.ACTION_HOVER_MOVE, 50f, 60f, 0);

        PointerIcon resolvedIcon = mCoordinator.onResolvePointerIcon(event, 0);

        assertEquals(mTestIcon, resolvedIcon);
        assertNotNull(mChildView.mReceivedEvent);

        // Verify that the child received the event and translated the coordinates
        // correctly so it is relative to the child.
        assertEquals(40f, mChildView.mReceivedEvent.getX(), 0.001f);
        assertEquals(50f, mChildView.mReceivedEvent.getY(), 0.001f);

        event.recycle();
    }

    @Test
    public void testOnResolvePointerIcon_returnNull_outsideViews() {
        MotionEvent event = MotionEvent.obtain(
                0, 0, MotionEvent.ACTION_HOVER_MOVE, 900f, 900f, 0);

        PointerIcon resolvedIcon = mCoordinator.onResolvePointerIcon(event, 0);

        assertNull(resolvedIcon);
        assertNull(mChildView.mReceivedEvent);

        event.recycle();
    }

    @Test
    public void testOnResolvePointerIcon_overlappingViews() {
        // bottomView is at the bottom (index 0).
        MockView bottomView = new MockView(mActivity);
        bottomView.mReturnIcon = mTestIcon;
        bottomView.layout(0, 0, 100, 100);
        bottomView.setVisibility(View.VISIBLE);

        // topView is on top (index 1).
        MockView topView = new MockView(mActivity);
        topView.mReturnIcon = null;
        topView.layout(0, 0, 100, 100);
        topView.setVisibility(View.VISIBLE);

        // Remove the default child view created in setUp().
        mCoordinator.removeAllViews();

        mCoordinator.addView(bottomView);
        mCoordinator.addView(topView);

        MotionEvent event = MotionEvent.obtain(
                0, 0, MotionEvent.ACTION_HOVER_MOVE, 50f, 50f, 0);

        PointerIcon resolvedIcon = mCoordinator.onResolvePointerIcon(event, 0);

        // Should pass through topView (null) and return bottomView's icon.
        assertEquals(mTestIcon, resolvedIcon);

        event.recycle();
    }

    @Test
    public void testOnResolvePoitnerIcon_overlappingViews_returnsFirstNonNullIcon() {
        // bottomView is at the bottom (index 0).
        MockView bottomView = new MockView(mActivity);
        bottomView.mReturnIcon = mTestIcon;
        bottomView.layout(0, 0, 100, 100);
        bottomView.setVisibility(View.VISIBLE);

        // topView is on top (index 1).
        MockView topView = new MockView(mActivity);
        topView.mReturnIcon = mCrosshairIcon;
        topView.layout(0, 0, 100, 100);
        topView.setVisibility(View.VISIBLE);

        mCoordinator.removeAllViews();
        mCoordinator.addView(bottomView);
        mCoordinator.addView(topView);

        MotionEvent event = MotionEvent.obtain(
                0, 0, MotionEvent.ACTION_HOVER_MOVE, 50f, 50f, 0);

        PointerIcon resolvedIcon = mCoordinator.onResolvePointerIcon(event, 0);

        // Should return topView's icon because it is on top.
        assertEquals(mCrosshairIcon, resolvedIcon);

        event.recycle();
    }

    @Test
    public void testOnResolvePointerIcon_ignoresInvisibleChild() {
        mChildView.layout(0, 0, 100, 100);
        mChildView.setVisibility(View.INVISIBLE);

        MotionEvent event = MotionEvent.obtain(
                0, 0, MotionEvent.ACTION_HOVER_MOVE, 50f, 50f, 0);

        PointerIcon resolvedIcon = mCoordinator.onResolvePointerIcon(event, 0);

        assertNull(resolvedIcon);
        assertNull(mChildView.mReceivedEvent);

        event.recycle();
    }
}
