// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;

import android.app.ActionBar.LayoutParams;
import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.DragEvent;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPhoneWindow;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.dragdrop.DragEventDispatchHelper.DragEventDispatchDestination;
import org.chromium.ui.widget.ChromePopupWindow;
import org.chromium.ui.widget.UiWidgetFactory;

import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link ContextMenuDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextMenuDialogUnitTest {
    static class TestDragDispatchingDestinationView extends View
            implements DragEventDispatchDestination {
        final List<DragEvent> mDispatchedDragEvents = new ArrayList<>();

        TestDragDispatchingDestinationView(Context context) {
            super(context);
        }

        @Override
        public View view() {
            return this;
        }

        @Override
        public boolean onDragEventWithOffset(DragEvent event, int dx, int dy) {
            mDispatchedDragEvents.add(event);
            return false;
        }
    }

    private static final int MENU_CONTENT_MIN_SIZE_PX = 100;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    ContextMenuDialog mDialog;

    Activity mActivity;
    FrameLayout mRootView;
    TestDragDispatchingDestinationView mDragDispatchingDestinationView;

    @Mock UiWidgetFactory mMockUiWidgetFactory;
    private ChromePopupWindow mSpyPopupWindow;
    FrameLayout mMenuContentView;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRootView = new FrameLayout(mActivity);
        TextView textView = new TextView(mActivity);
        textView.setText("Test String");

        mMenuContentView = new FrameLayout(mActivity);
        mMenuContentView.addView(textView);
        // Ensure the popup is large enough to satisfy AnchoredPopupWindow's minimal size check.
        mMenuContentView.setMinimumWidth(MENU_CONTENT_MIN_SIZE_PX);
        mMenuContentView.setMinimumHeight(MENU_CONTENT_MIN_SIZE_PX);

        mActivity.setContentView(
                mRootView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        mSpyPopupWindow = Mockito.spy(UiWidgetFactory.getInstance().createPopupWindow(mActivity));
        mDragDispatchingDestinationView = new TestDragDispatchingDestinationView(mActivity);
        UiWidgetFactory.setInstance(mMockUiWidgetFactory);
        Mockito.when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        Mockito.doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());
        Mockito.doNothing().when(mSpyPopupWindow).dismiss();
    }

    @After
    public void tearDown() {
        AccessibilityStateTestHelper.setIsKnownScreenReaderEnabledForTesting(false);
        UiWidgetFactory.setInstance(null);
        mActivity.finish();
    }

    @Test
    public void testCreate_usePopupStyle() {
        mDialog = createContextMenuDialog(/* isPopup= */ false, /* shouldRemoveScrim= */ true);
        mDialog.show();

        ShadowPhoneWindow window = (ShadowPhoneWindow) Shadows.shadowOf(mDialog.getWindow());
        Assert.assertTrue(
                "FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS not in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS));
        Assert.assertTrue(
                "FLAG_NOT_TOUCH_MODAL not in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL));
        Assert.assertFalse(
                "FLAG_DIM_BEHIND is in flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_DIM_BEHIND));

        Assert.assertEquals(
                "Dialog status bar color should match activity status bar color.",
                mActivity.getWindow().getStatusBarColor(),
                mDialog.getWindow().getStatusBarColor());
        Assert.assertEquals(
                "Dialog navigation bar color should match activity navigation bar color.",
                mActivity.getWindow().getNavigationBarColor(),
                mDialog.getWindow().getNavigationBarColor());
    }

    @Test
    public void testCreateDialog_useRegularStyle() {
        mDialog = createContextMenuDialog(/* isPopup= */ false, /* shouldRemoveScrim= */ false);
        mDialog.show();

        // Only checks the flag is unset to make sure the setup for |shouldRemoveScrim| is not ran.
        ShadowPhoneWindow window = (ShadowPhoneWindow) Shadows.shadowOf(mDialog.getWindow());
        Assert.assertFalse(
                "FLAG_NOT_TOUCH_MODAL is in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL));
    }



    @Test
    public void testShowPopupWindow() {
        mDialog = createContextMenuDialog(/* isPopup= */ true, /* shouldRemoveScrim= */ false);
        mDialog.show();
        requestLayoutForRootView();

        final ArgumentCaptor<Integer> gravityCaptor = ArgumentCaptor.forClass(Integer.class);
        Mockito.verify(mSpyPopupWindow)
                .showAtLocation(any(View.class), gravityCaptor.capture(), anyInt(), anyInt());

        Assert.assertEquals(
                "Popup gravity should have Gravity.START.",
                Gravity.START,
                (gravityCaptor.getValue() & Gravity.START));
        Assert.assertEquals(
                "Popup gravity should have Gravity.TOP.",
                Gravity.TOP,
                (gravityCaptor.getValue() & Gravity.TOP),
                Gravity.TOP);

        mDialog.dismiss();
        Mockito.verify(mSpyPopupWindow).dismiss();
    }

    @Test
    public void testShowPopupWindow_2ndLayout() {
        mDialog = createContextMenuDialog(/* isPopup= */ true, /* shouldRemoveScrim= */ false);
        mDialog.show();
        // Change layout params and request layout so #onLayoutChange is triggered.
        requestLayoutForRootView();
        Mockito.verify(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());

        // Mock up popup window is showing.
        Mockito.doReturn(true).when(mSpyPopupWindow).isShowing();

        requestLayoutForRootView();
        Mockito.verify(mSpyPopupWindow).dismiss();
    }

    /**
     * Inspired by https://crbug.com/1281011. If popup context menu is dismissed before
     * #onLayoutRequest for the root view, popup menu should not get invoked.
     */
    @Test
    public void testShowPopupWindow_BeforeOnLayout() {
        mDialog = createContextMenuDialog(/* isPopup= */ true, /* shouldRemoveScrim= */ false);
        mDialog.show();

        mDialog.dismiss();
        // Spy popup is not invoked because the dialog does not manage to create the popup window.
        Mockito.verify(mSpyPopupWindow, Mockito.times(0)).dismiss();
    }

    @Test
    public void testShowPopupWindow_NotFocusableInA11y() throws Exception {
        AccessibilityStateTestHelper.setIsKnownScreenReaderEnabledForTesting(true);

        mDialog = createContextMenuDialog(/* isPopup= */ true, /* shouldRemoveScrim= */ false);
        mDialog.show();
        // Change layout params and request layout so #onLayoutChange is triggered.
        requestLayoutForRootView();

        Mockito.verify(mSpyPopupWindow).setFocusable(eq(true));
    }

    @Test
    public void testDispatchTouchToDelegate() {
        mDialog = createContextMenuDialog(/* isPopup= */ true, /* shouldRemoveScrim= */ true);
        mDialog.show();
        requestLayoutForRootView();
        Mockito.verify(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());
        attachDragDispatchingDestinationView();

        List<MotionEvent> dispatchedEvents = new ArrayList<>();
        mDragDispatchingDestinationView.setOnTouchListener(
                (v, event) -> {
                    dispatchedEvents.add(event);
                    return true;
                });

        // common motion events other than ACTION_DOWN should be forwarded to touch event delegate.
        int[] motionEvenActions =
                new int[] {
                    MotionEvent.ACTION_CANCEL,
                    MotionEvent.ACTION_HOVER_ENTER,
                    MotionEvent.ACTION_HOVER_EXIT,
                    MotionEvent.ACTION_HOVER_MOVE,
                    MotionEvent.ACTION_MOVE,
                    MotionEvent.ACTION_OUTSIDE,
                    MotionEvent.ACTION_POINTER_DOWN,
                    MotionEvent.ACTION_POINTER_UP,
                    MotionEvent.ACTION_SCROLL,
                    MotionEvent.ACTION_UP
                };
        for (int actionType : motionEvenActions) {
            MotionEvent event = createMotionEventWithActionType(actionType);
            mDialog.onTouchEvent(event);
            Assert.assertEquals(
                    "Action" + actionType + " should be dispatched.",
                    event,
                    dispatchedEvents.get(dispatchedEvents.size() - 1));
        }
        Assert.assertEquals(motionEvenActions.length, dispatchedEvents.size());

        // ACTION_DOWN should dismiss the dialog and the popup window.
        MotionEvent downEvent = createMotionEventWithActionType(MotionEvent.ACTION_DOWN);
        mDialog.onTouchEvent(downEvent);
        Assert.assertFalse(dispatchedEvents.contains(downEvent));
        Mockito.verify(mSpyPopupWindow).dismiss();
    }

    @Test
    public void testDispatchDragEvents() {
        mDialog = createContextMenuDialog(/* isPopup= */ true, /* shouldRemoveScrim= */ true);
        mDialog.show();
        requestLayoutForRootView();
        Mockito.verify(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());
        Assert.assertNotNull("OnDragListener is null.", mDialog.getOnDragListenerForTesting());

        final DragEvent mockDragEvent = Mockito.mock(DragEvent.class);
        Mockito.doReturn(DragEvent.ACTION_DRAG_LOCATION).when(mockDragEvent).getAction();

        attachDragDispatchingDestinationView();
        mDialog.getOnDragListenerForTesting().onDrag(mRootView, mockDragEvent);
        Assert.assertEquals(
                List.of(mockDragEvent), mDragDispatchingDestinationView.mDispatchedDragEvents);

        final DragEvent mockDragEvent2 = Mockito.mock(DragEvent.class);
        Mockito.doReturn(DragEvent.ACTION_DRAG_LOCATION).when(mockDragEvent2).getAction();
        mRootView.removeView(mDragDispatchingDestinationView);
        mDialog.getOnDragListenerForTesting().onDrag(mRootView, mockDragEvent2);
        Assert.assertEquals(
                List.of(mockDragEvent), mDragDispatchingDestinationView.mDispatchedDragEvents);
    }

    private ContextMenuDialog createContextMenuDialog(boolean isPopup, boolean shouldRemoveScrim) {
        return new ContextMenuDialog(
                mActivity,
                /* windowAndroid= */ null,
                /* theme= */ 0,
                ContextMenuDialog.NO_CUSTOM_MARGIN,
                ContextMenuDialog.NO_CUSTOM_MARGIN,
                mRootView,
                mMenuContentView,
                isPopup,
                /* isFlyout= */ false,
                shouldRemoveScrim,
                /* popupMargin= */ 0,
                mDragDispatchingDestinationView,
                new Rect(0, 0, 0, 0),
                /* shouldPadForWindowInsets= */ true,
                /* onDismissCallback= */ null,
                /* flyoutExtraPaddingY= */ 0);
    }

    private void requestLayoutForRootView() {
        // Change layout params and request layout so #onLayoutChange is triggered.
        mRootView.setRight(mRootView.getRight() + 1);
        mRootView.requestLayout();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void attachDragDispatchingDestinationView() {
        mRootView.addView(mDragDispatchingDestinationView);
        Assert.assertTrue(mDragDispatchingDestinationView.isAttachedToWindow());
    }

    private static MotionEvent createMotionEventWithActionType(int actionType) {
        return MotionEvent.obtain(
                /* downTime= */ 0,
                /* eventTime= */ 0,
                actionType,
                /* x= */ 0,
                /* y= */ 0,
                /* metaState= */ 0);
    }
}
