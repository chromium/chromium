// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ExtensionActionDragHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionActionDragHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock ItemTouchHelper mItemTouchHelper;

    private ListMenuButton mItemView;
    private RecyclerView.ViewHolder mViewHolder;
    private int mClickCount;
    private int mLongClickCount;
    private Activity mActivity;
    private ExtensionActionDragHelper mDragHelper;
    private int mTouchSlop;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mItemView = new ListMenuButton(mActivity, null);
        mItemView.setOnClickListener(v -> mClickCount++);
        mItemView.setOnLongClickListener(
                v -> {
                    mLongClickCount++;
                    return true;
                });
        mViewHolder = new RecyclerView.ViewHolder(mItemView) {};

        mTouchSlop = ViewConfiguration.get(mActivity).getScaledTouchSlop();

        mDragHelper = new ExtensionActionDragHelper(mActivity, mItemTouchHelper, mViewHolder);
    }

    @Test
    public void testTouch_Click() {
        mDragHelper.onTouch(
                mItemView, obtainTouchEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        assertTrue(mItemView.isPressed());

        // Advance time less than long press threshold (e.g., 100ms).
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        mDragHelper.onTouch(
                mItemView, obtainTouchEvent(MotionEvent.ACTION_UP, /* x= */ 50f, /* y= */ 50f));

        // Verify click performed and pressed state cleared.
        assertEquals(1, mClickCount);
        assertFalse(mItemView.isPressed());

        // Verify no drag or long click occurred.
        verify(mItemTouchHelper, never()).startDrag(any());
        assertEquals(0, mLongClickCount);
    }

    @Test
    public void testTouch_LongPress() {
        mDragHelper.onTouch(
                mItemView, obtainTouchEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        // Advance time past the system long press threshold.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // Verify Long Click triggered.
        assertEquals(1, mLongClickCount);

        mDragHelper.onTouch(
                mItemView, obtainTouchEvent(MotionEvent.ACTION_UP, /* x= */ 50f, /* y= */ 50f));

        assertEquals(0, mClickCount);
        assertFalse(mItemView.isPressed());
    }

    @Test
    public void testTouch_TouchDrag() {
        // Like production, show the context menu on long press.
        AnchoredPopupWindow.setShowHookForTesting(() -> {});
        mActivity.setContentView(mItemView);
        mItemView.setDelegate(
                () ->
                        BrowserUiListMenuUtils.getBasicListMenu(
                                mActivity, new ModelList(), /* delegate= */ null),
                /* overrideOnClickListener= */ false);
        mItemView.setOnLongClickListener(
                v -> {
                    mItemView.showMenu();
                    return true;
                });

        mDragHelper.onTouch(
                mItemView, obtainTouchEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        // Move before the longpress threshold. Nothing should happen, because on touch drag should
        // require longpress.
        float bigMove = mTouchSlop + 10.0f;
        mDragHelper.onTouch(
                mItemView,
                obtainTouchEvent(MotionEvent.ACTION_MOVE, /* x= */ 50f + bigMove, /* y= */ 50f));
        verify(mItemTouchHelper, never()).startDrag(any());

        // Advance time past the system longpress threshold.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertTrue(mItemView.getHost().isMenuShowing());

        // Move slightly (within slop). Nothing should happen.
        float smallMove = mTouchSlop / 2.0f;
        mDragHelper.onTouch(
                mItemView,
                obtainTouchEvent(MotionEvent.ACTION_MOVE, /* x= */ 50f + smallMove, /* y= */ 50f));
        verify(mItemTouchHelper, never()).startDrag(any());

        // Move beyond the touch slop.
        mDragHelper.onTouch(
                mItemView,
                obtainTouchEvent(MotionEvent.ACTION_MOVE, /* x= */ 50f + bigMove, /* y= */ 50f));

        // Verify drag started and pressed state cleared.
        verify(mItemTouchHelper).startDrag(mViewHolder);
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(mItemView.isPressed());

        // Verify that starting a drag dismisses the context menu.
        assertFalse(mItemView.getHost().isMenuShowing());
    }

    @Test
    public void testTouch_MouseDrag() {
        mDragHelper.onTouch(
                mItemView, obtainMouseEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        // Move slightly (within slop). Nothing should happen.
        float smallMove = mTouchSlop / 2.0f;
        mDragHelper.onTouch(
                mItemView,
                obtainMouseEvent(MotionEvent.ACTION_MOVE, /* x= */ 50f + smallMove, /* y= */ 50f));
        verify(mItemTouchHelper, never()).startDrag(any());

        // Move beyond the touch slop.
        float bigMove = mTouchSlop + 10.0f;
        mDragHelper.onTouch(
                mItemView,
                obtainMouseEvent(MotionEvent.ACTION_MOVE, /* x= */ 50f + bigMove, /* y= */ 50f));

        // Verify drag started and pressed state cleared even without longpress.
        verify(mItemTouchHelper).startDrag(mViewHolder);
        assertFalse(mItemView.isPressed());
    }

    @Test
    public void testTouch_Cancel() {
        mDragHelper.onTouch(
                mItemView, obtainTouchEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f));

        mDragHelper.onTouch(
                mItemView, obtainTouchEvent(MotionEvent.ACTION_CANCEL, /* x= */ 50f, /* y= */ 50f));

        assertFalse(mItemView.isPressed());
        assertEquals(0, mClickCount);

        // Ensure timer is killed.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(0, mLongClickCount);
    }

    @Test
    public void testSecondaryClick_Ignored() {
        MotionEvent event = obtainTouchEvent(MotionEvent.ACTION_DOWN, /* x= */ 50f, /* y= */ 50f);

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
        assertFalse(mItemView.isPressed());
    }

    private MotionEvent obtainTouchEvent(int action, float x, float y) {
        return obtainEvent(
                action, x, y, InputDevice.SOURCE_TOUCHSCREEN, MotionEvent.TOOL_TYPE_FINGER);
    }

    private MotionEvent obtainMouseEvent(int action, float x, float y) {
        return obtainEvent(action, x, y, InputDevice.SOURCE_MOUSE, MotionEvent.TOOL_TYPE_MOUSE);
    }

    private MotionEvent obtainEvent(int action, float x, float y, int source, int toolType) {
        long time = SystemClock.uptimeMillis();
        return MotionEventTestUtils.createMotionEvent(time, time, action, x, y, source, toolType);
    }
}
