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
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.PopupWindow;
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
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowPhoneWindow;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.dragdrop.DragEventDispatchHelper.DragEventDispatchDestination;
import org.chromium.ui.widget.UiWidgetFactory;

/** Unit test for {@link ContextMenuDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPhoneWindow.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class ContextMenuDialogUnitTest {
    private static final int DIALOG_SIZE_DIP = 50;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    ContextMenuDialog mDialog;

    Activity mActivity;
    FrameLayout mMenuContentView;
    View mRootView;
    TestDragDispatchingDestinationView mSpyDragDispatchingDestinationView;

    @Mock UiWidgetFactory mMockUiWidgetFactory;
    @Spy PopupWindow mSpyPopupWindow;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRootView = new FrameLayout(mActivity);
        TextView textView = new TextView(mActivity);
        textView.setText("Test String");
        mMenuContentView = new FrameLayout(mActivity);
        mMenuContentView.addView(textView);
        mActivity.setContentView(
                mRootView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        mSpyPopupWindow = Mockito.spy(UiWidgetFactory.getInstance().createPopupWindow(mActivity));
        mSpyDragDispatchingDestinationView =
                Mockito.spy(new TestDragDispatchingDestinationView(mActivity));
        UiWidgetFactory.setInstance(mMockUiWidgetFactory);
        Mockito.when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);
        Mockito.doNothing()
                .when(mSpyPopupWindow)
                .showAtLocation(any(View.class), anyInt(), anyInt(), anyInt());
        Mockito.doNothing().when(mSpyPopupWindow).dismiss();

        View mockContentView = Mockito.mock(ViewGroup.class);
        Mockito.when(mockContentView.getMeasuredHeight()).thenReturn(DIALOG_SIZE_DIP);
        Mockito.when(mockContentView.getMeasuredWidth()).thenReturn(DIALOG_SIZE_DIP);
        Mockito.doReturn(mockContentView).when(mSpyPopupWindow).getContentView();
    }

    @After
    public void tearDown() {
        AccessibilityState.setIsScreenReaderEnabledForTesting(false);
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
    public void testCreateDialog_dontMatchSysUi() {
        mDialog =
                createContextMenuDialog(
                        /* isPopup= */ false,
                        /* shouldRemoveScrim= */ false,
                        /* shouldSysUiMatchActivity */ false);
        mDialog.show();

        // Only checks the flag is unset to make sure the setup for |shouldSysUiMatchActivity| is
        // not ran.
        ShadowPhoneWindow window = (ShadowPhoneWindow) Shadows.shadowOf(mDialog.getWindow());
        Assert.assertFalse(
                "FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS is in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS));
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
                .showAtLocation(
                        eq(mRootView.getRootView()), gravityCaptor.capture(), anyInt(), anyInt());

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
                .showAtLocation(eq(mRootView.getRootView()), anyInt(), anyInt(), anyInt());

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
        AccessibilityState.setIsScreenReaderEnabledForTesting(true);

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
                .showAtLocation(eq(mRootView.getRootView()), anyInt(), anyInt(), anyInt());
        Mockito.doReturn(true).when(mSpyDragDispatchingDestinationView).isAttachedToWindow();

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
            MotionEvent event = createMockMotionEventWithActionType(actionType);
            mDialog.onTouchEvent(event);
            Mockito.verify(
                            mSpyDragDispatchingDestinationView,
                            Mockito.description("Action" + actionType))
                    .dispatchTouchEvent(eq(event));
        }

        // ACTION_DOWN should dismiss the dialog and the popup window.
        MotionEvent downEvent = createMockMotionEventWithActionType(MotionEvent.ACTION_DOWN);
        mDialog.onTouchEvent(downEvent);
        Mockito.verify(mSpyDragDispatchingDestinationView, Mockito.times(0))
                .dispatchTouchEvent(eq(downEvent));
        Mockito.verify(mSpyPopupWindow).dismiss();
    }

    @Test
    public void testDispatchDragEvents() {
        mDialog = createContextMenuDialog(/* isPopup= */ true, /* shouldRemoveScrim= */ true);
        mDialog.show();
        requestLayoutForRootView();
        Mockito.verify(mSpyPopupWindow)
                .showAtLocation(eq(mRootView.getRootView()), anyInt(), anyInt(), anyInt());
        Assert.assertNotNull("OnDragListener is null.", mDialog.getOnDragListenerForTesting());

        final DragEvent mockDragEvent = Mockito.mock(DragEvent.class);
        Mockito.doReturn(DragEvent.ACTION_DRAG_LOCATION).when(mockDragEvent).getAction();

        Mockito.doReturn(true).when(mSpyDragDispatchingDestinationView).isAttachedToWindow();
        mDialog.getOnDragListenerForTesting().onDrag(mRootView, mockDragEvent);
        Mockito.verify(mSpyDragDispatchingDestinationView, Mockito.times(1))
                .onDragEventWithOffset(eq(mockDragEvent), anyInt(), anyInt());

        final DragEvent mockDragEvent2 = Mockito.mock(DragEvent.class);
        Mockito.doReturn(false).when(mSpyDragDispatchingDestinationView).isAttachedToWindow();
        mDialog.getOnDragListenerForTesting().onDrag(mRootView, mockDragEvent2);
        Mockito.verify(mSpyDragDispatchingDestinationView, Mockito.times(0))
                .onDragEventWithOffset(eq(mockDragEvent2), anyInt(), anyInt());
    }

    private ContextMenuDialog createContextMenuDialog(boolean isPopup, boolean shouldRemoveScrim) {
        return createContextMenuDialog(isPopup, shouldRemoveScrim, true);
    }

    private ContextMenuDialog createContextMenuDialog(
            boolean isPopup, boolean shouldRemoveScrim, boolean shouldSysUiMatchActivity) {
        return new ContextMenuDialog(
                mActivity,
                0,
                ContextMenuDialog.NO_CUSTOM_MARGIN,
                ContextMenuDialog.NO_CUSTOM_MARGIN,
                mRootView,
                mMenuContentView,
                isPopup,
                shouldRemoveScrim,
                shouldSysUiMatchActivity,
                0,
                0,
                mSpyDragDispatchingDestinationView,
                new Rect(0, 0, 0, 0));
    }

    private void requestLayoutForRootView() {
        // Change layout params and request layout so #onLayoutChange is triggered.
        mRootView.setRight(mRootView.getRight() + 1);
        mRootView.requestLayout();
    }

    private MotionEvent createMockMotionEventWithActionType(int actionType) {
        MotionEvent motionEvent = Mockito.mock(MotionEvent.class);
        Mockito.doReturn(actionType).when(motionEvent).getAction();

        return motionEvent;
    }

    static class TestDragDispatchingDestinationView extends View
            implements DragEventDispatchDestination {
        public TestDragDispatchingDestinationView(Context context) {
            super(context);
        }

        @Override
        public View view() {
            return this;
        }

        @Override
        public boolean onDragEventWithOffset(DragEvent event, int dx, int dy) {
            return false;
        }
    }
}
