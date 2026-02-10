// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ExtensionActionDragHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExtensionActionDragHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock ItemTouchHelper mItemTouchHelper;
    @Mock View mItemView;

    RecyclerView.ViewHolder mViewHolder;
    private Activity mActivity;
    private ExtensionActionDragHelper mDragHelper;
    private int mTouchSlop;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mViewHolder = new RecyclerView.ViewHolder(mItemView) {};

        when(mItemView.getContext()).thenReturn(mActivity);

        mTouchSlop = ViewConfiguration.get(mActivity).getScaledTouchSlop();

        mDragHelper = new ExtensionActionDragHelper(mActivity, mItemTouchHelper, mViewHolder);
    }

    @Test
    public void testTouch_Click() {
        mDragHelper.onTouch(
                mItemView, obtainEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        verify(mItemView).setPressed(true);

        // Advance time less than long press threshold (e.g., 100ms).
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        mDragHelper.onTouch(
                mItemView, obtainEvent(MotionEvent.ACTION_UP, /* x= */ 50f, /* y= */ 50f));

        // Verify click performed and pressed state cleared.
        verify(mItemView).performClick();
        verify(mItemView).setPressed(false);

        // Verify no drag or long click occurred.
        verify(mItemTouchHelper, never()).startDrag(any());
        verify(mItemView, never()).performLongClick();
    }

    @Test
    public void testTouch_LongPress() {
        mDragHelper.onTouch(
                mItemView, obtainEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        // Advance time past the system long press threshold.
        ShadowLooper.idleMainLooper(ViewConfiguration.getLongPressTimeout(), TimeUnit.MILLISECONDS);

        // Verify Long Click triggered.
        verify(mItemView).performLongClick();

        mDragHelper.onTouch(
                mItemView, obtainEvent(MotionEvent.ACTION_UP, /* x= */ 50f, /* y= */ 50f));

        verify(mItemView, never()).performClick();
        verify(mItemView).setPressed(false);
    }

    @Test
    public void testTouch_Drag() {
        mDragHelper.onTouch(
                mItemView, obtainEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        // Move slightly (within slop). Nothing should happen.
        float smallMove = mTouchSlop / 2.0f;
        mDragHelper.onTouch(
                mItemView,
                obtainEvent(MotionEvent.ACTION_MOVE, /* x= */ 50f + smallMove, /* y= */ 50f));

        verify(mItemTouchHelper, never()).startDrag(any());

        float bigMove = mTouchSlop + 10.0f;
        mDragHelper.onTouch(
                mItemView,
                obtainEvent(MotionEvent.ACTION_MOVE, /* x= */ 50f + bigMove, /* y= */ 50f));

        // Verify drag started and pressed state cleared.
        verify(mItemTouchHelper).startDrag(mViewHolder);
        verify(mItemView).setPressed(false);

        // Verify drag cancels the long press timer (advance time to check).
        ShadowLooper.idleMainLooper(
                ViewConfiguration.getLongPressTimeout() * 2, TimeUnit.MILLISECONDS);
        verify(mItemView, never()).performLongClick();
    }

    @Test
    public void testTouch_Cancel() {
        mDragHelper.onTouch(
                mItemView, obtainEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        mDragHelper.onTouch(
                mItemView, obtainEvent(MotionEvent.ACTION_CANCEL, /* x= */ 50f, /* y= */ 50f));

        verify(mItemView).setPressed(false);
        verify(mItemView, never()).performClick();

        // Ensure timer is killed.
        ShadowLooper.idleMainLooper(
                ViewConfiguration.getLongPressTimeout() * 2, TimeUnit.MILLISECONDS);
        verify(mItemView, never()).performLongClick();
    }

    @Test
    public void testSecondaryClick_Ignored() {
        MotionEvent event = obtainEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f);

        // Create and send a fake context click event.
        long downTime = SystemClock.uptimeMillis();
        MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[1];
        props[0] = new MotionEvent.PointerProperties();
        props[0].id = 0;
        props[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = 50f;
        coords[0].y = 50f;

        MotionEvent rightClick =
                MotionEvent.obtain(
                        downTime,
                        downTime,
                        MotionEvent.ACTION_DOWN,
                        /* pointerCount= */ 1,
                        props,
                        coords,
                        /* metaState= */ 0,
                        MotionEvent.BUTTON_SECONDARY,
                        /* xPrecision= */ 1.0f,
                        /* yPrecision= */ 1.0f,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        /* source= */ 0,
                        /* flags= */ 0);

        boolean consumed = mDragHelper.onTouch(mItemView, rightClick);

        // The event should be process by Android, not the helper.
        assert !consumed;
        verify(mItemView, never()).setPressed(true);
    }

    private MotionEvent obtainEvent(int action, float x, float y) {
        long time = SystemClock.uptimeMillis();
        return MotionEventTestUtils.createMotionEvent(
                time,
                time,
                action,
                x,
                y,
                InputDevice.SOURCE_TOUCHSCREEN,
                MotionEvent.TOOL_TYPE_FINGER);
    }
}
