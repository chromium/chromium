// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.DeviceInput;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.BAKLAVA)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@EnableFeatures(OmniboxFeatureList.OMNIBOX_PREFETCH_SELECTED_SUGGESTIONS_OMT_ANDROID)
public class SuggestionsOmtOverlayViewUnitTest {
    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private Context mContext;
    private FrameLayout mParent;
    private OmniboxSuggestionsDropdown mDropdown;
    private SuggestionsOmtOverlayView mOverlayView;

    @Before
    public void setUp() {
        DeviceInput.setSupportsPrecisionPointerForTesting(false);
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        mActivityController.windowFocusChanged(true);
        mActivity = mActivityController.get();
        mContext = new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);
        mParent = new FrameLayout(mContext);
        mActivity.setContentView(mParent);
        RobolectricUtil.runAllBackgroundAndUi();
        mDropdown =
                new OmniboxSuggestionsDropdown(
                        mContext, null, new SuggestionLayoutScrollListener(mContext));
        mOverlayView = new SuggestionsOmtOverlayView(mContext, mDropdown);
    }

    @After
    public void tearDown() {
        mOverlayView.destroy();
        mDropdown.destroy();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void configureVisibleStationaryDropdown(OmniboxSuggestionsDropdown dropdown) {
        dropdown.setOmtOverlayView(mOverlayView);
        mParent.addView(dropdown, new FrameLayout.LayoutParams(100, 200));
        mOverlayView.attachToParent(mParent);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testOverlayLifecycle_threadAndAttachment() throws Exception {
        assertNotNull(mOverlayView.getInputThreadForTesting());
        assertNotNull(mOverlayView.getInputHandlerForTesting());
        assertTrue(mOverlayView.getInputThreadForTesting().isAlive());

        mOverlayView.attachToParent(mParent);
        assertEquals(1, mParent.getChildCount());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.destroy();
        assertEquals(0, mParent.getChildCount());
        assertFalse(mOverlayView.isTouchTransferActive());
        mOverlayView.getInputThreadForTesting().join();
        assertFalse(mOverlayView.getInputThreadForTesting().isAlive());
    }

    @Test
    public void testOverlayBounds_initialAttachmentAndBoundsUpdate() {
        mParent.addView(mDropdown, new FrameLayout.LayoutParams(0, 0));
        mOverlayView.attachToParent(mParent);
        RobolectricUtil.runAllBackgroundAndUi();

        var layoutParams = mOverlayView.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(0, layoutParams.width);
        assertEquals(0, layoutParams.height);
        assertEquals(0f, mOverlayView.getTranslationX(), /* delta= */ 0f);
        assertEquals(0f, mOverlayView.getTranslationY(), /* delta= */ 0f);
        assertEquals(View.GONE, mOverlayView.getVisibility());

        mDropdown.layout(10, 20, 110, 220);

        layoutParams = mOverlayView.getLayoutParams();
        assertEquals(100, layoutParams.width);
        assertEquals(200, layoutParams.height);
        assertEquals(10f, mOverlayView.getTranslationX(), /* delta= */ 0f);
        assertEquals(20f, mOverlayView.getTranslationY(), /* delta= */ 0f);
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayBounds_synchronizesOnDropdownResize() {
        configureVisibleStationaryDropdown(mDropdown);

        var layoutParams = mOverlayView.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(100, layoutParams.width);
        assertEquals(200, layoutParams.height);

        mDropdown.layout(0, 0, 300, 450);

        layoutParams = mOverlayView.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(300, layoutParams.width);
        assertEquals(450, layoutParams.height);
    }

    @Test
    public void testOverlayBounds_synchronizesWhenDropdownCollapses() {
        configureVisibleStationaryDropdown(mDropdown);
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mDropdown.layout(0, 0, 100, 0);

        var layoutParams = mOverlayView.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(100, layoutParams.width);
        assertEquals(0, layoutParams.height);
        assertEquals(View.GONE, mOverlayView.getVisibility());

        mDropdown.layout(0, 0, 100, 200);
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mDropdown.layout(0, 0, 0, 200);

        layoutParams = mOverlayView.getLayoutParams();
        assertEquals(0, layoutParams.width);
        assertEquals(200, layoutParams.height);
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_visibleWhenDropdownStationaryAndNoPointer() {
        configureVisibleStationaryDropdown(mDropdown);
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_hiddenWhenDropdownNotVisible() {
        configureVisibleStationaryDropdown(mDropdown);
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mDropdown.setVisibility(View.GONE);
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_hiddenWhenDropdownNotShown() {
        configureVisibleStationaryDropdown(mDropdown);
        assertTrue(mDropdown.isShown());
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mParent.setVisibility(View.INVISIBLE);

        assertFalse(mDropdown.isShown());
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_hiddenWhenWindowNotVisible() {
        configureVisibleStationaryDropdown(mDropdown);
        assertEquals(View.VISIBLE, mDropdown.getWindowVisibility());
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mActivity.getWindow().getDecorView().setVisibility(View.GONE);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(View.GONE, mDropdown.getWindowVisibility());
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_hiddenWhenWindowNotFocused() {
        configureVisibleStationaryDropdown(mDropdown);
        assertTrue(mDropdown.hasWindowFocus());
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mActivityController.windowFocusChanged(false);

        assertFalse(mDropdown.hasWindowFocus());
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_remainsVisibleWhenWindowNotFocusedDuringTouchTransfer() {
        configureVisibleStationaryDropdown(mDropdown);
        assertTrue(mDropdown.hasWindowFocus());
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mOverlayView.onTouchTransferred();
        mActivityController.windowFocusChanged(false);
        mOverlayView.updateOverlayVisualState();

        assertFalse(mDropdown.hasWindowFocus());
        assertTrue(mOverlayView.isTouchTransferActive());
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        mOverlayView.onGestureEnded();
        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mOverlayView.isTouchTransferActive());
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_hiddenWhenPointingDeviceConnected() {
        configureVisibleStationaryDropdown(mDropdown);
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        mOverlayView.updateOverlayVisualState();
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_hiddenDuringScroll() {
        configureVisibleStationaryDropdown(mDropdown);
        assertEquals(RecyclerView.SCROLL_STATE_IDLE, mDropdown.getScrollState());
        assertEquals(View.VISIBLE, mOverlayView.getVisibility());

        int touchSlop = ViewConfiguration.get(mContext).getScaledTouchSlop();
        var down =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_DOWN,
                        10f,
                        100f,
                        0);
        var move =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 10,
                        MotionEvent.ACTION_MOVE,
                        10f,
                        100f - 2 * touchSlop,
                        0);
        mDropdown.dispatchTouchEvent(down);
        mDropdown.dispatchTouchEvent(move);
        down.recycle();
        move.recycle();

        assertEquals(RecyclerView.SCROLL_STATE_DRAGGING, mDropdown.getScrollState());
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayVisibility_hiddenWhenBoundsEmpty() {
        configureVisibleStationaryDropdown(mDropdown);

        assertEquals(View.VISIBLE, mOverlayView.getVisibility());
        mDropdown.layout(0, 0, 0, 200);
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayTouchTransfer_stateManagement() {
        assertFalse(mOverlayView.isTouchTransferActive());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.resetTouchTransfer();
        assertFalse(mOverlayView.isTouchTransferActive());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.onGestureEnded();
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(mOverlayView.isTouchTransferActive());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.surfaceDestroyed(mOverlayView.getHolder());
        assertFalse(mOverlayView.isTouchTransferActive());
    }

    @Test
    public void testFocusSuppression_ignoresFocusLossWhileTouchTransferActive() {
        var child = new View(mContext);
        child.setPressed(true);
        mDropdown.addView(child);
        mDropdown.setOmtOverlayView(mOverlayView);
        mOverlayView.onTouchTransferred();

        mDropdown.dispatchWindowFocusChanged(false);
        assertTrue(child.isPressed());
    }

    @Test
    public void testFocusSuppression_forwardsFocusLossWhenTouchTransferNotActive() {
        var child = new View(mContext);
        child.setPressed(true);
        mDropdown.addView(child);
        mDropdown.setOmtOverlayView(mOverlayView);

        assertFalse(mOverlayView.isTouchTransferActive());
        mDropdown.dispatchWindowFocusChanged(false);
        assertFalse(child.isPressed());
    }
}
