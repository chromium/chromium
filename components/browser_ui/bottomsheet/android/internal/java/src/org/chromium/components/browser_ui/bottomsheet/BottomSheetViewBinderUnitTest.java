// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import androidx.core.view.ViewCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.SheetLayoutMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.TouchHandler;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock TouchHandler mTouchHandler;
    private @Mock OnClickListener mClickListener;
    private @Mock Runnable mCallback;
    private @Mock AccessibilityDelegate mAccessibilityDelegate;
    private Context mContext;
    private PropertyModel mModel;
    private BottomSheetView mView;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;
        FrameLayout container = new FrameLayout(mContext);
        mView =
                (BottomSheetView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.bottom_sheet_desktop, container, false);
        container.addView(mView);
        activity.setContentView(container);
        mModel = new PropertyModel(BottomSheetProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(mModel, mView, BottomSheetViewBinder::bind);
    }

    @Test
    public void testPropertyBinding() {
        GlowSpec spec = new GlowSpec(Color.RED, GlowSpec.ShadowSize.LONG);
        mModel.set(BottomSheetProperties.GLOW_SPEC, spec);
        assertEquals(spec, mView.getGlowSpec());

        mModel.set(BottomSheetProperties.SHEET_LAYOUT_MODE, SheetLayoutMode.DESKTOP_POPUP);
        assertEquals(SheetLayoutMode.DESKTOP_POPUP, mView.getSheetLayoutMode());

        View background = mView.findViewById(R.id.background);
        mModel.set(BottomSheetProperties.BACKGROUND_COLOR, Color.GREEN);
        assertEquals(ColorStateList.valueOf(Color.GREEN), background.getBackgroundTintList());

        View closeButton = mView.findViewById(R.id.bottom_sheet_close_button);
        mModel.set(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY, true);
        assertEquals(View.VISIBLE, closeButton.getVisibility());

        OnClickListener listener = mock(OnClickListener.class);
        mModel.set(BottomSheetProperties.CLOSE_BUTTON_CLICK_LISTENER, listener);
        closeButton.performClick();
        verify(listener).onClick(any());

        mModel.set(BottomSheetProperties.CONTAINER_TOUCH_ENABLED, false);
        TouchRestrictingFrameLayout contentContainer =
                mView.findViewById(R.id.bottom_sheet_content);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        assertTrue(contentContainer.onInterceptTouchEvent(event));

        View contentView = new View(mContext);
        mModel.set(BottomSheetProperties.CONTENT_VIEW, contentView);
        assertEquals(1, contentContainer.getChildCount());
        assertEquals(contentView, contentContainer.getChildAt(0));

        View toolbarView = new View(mContext);
        mModel.set(BottomSheetProperties.TOOLBAR_VIEW, toolbarView);
        ViewGroup toolbarContainer = mView.findViewById(R.id.bottom_sheet_toolbar_container);
        assertEquals(1, toolbarContainer.getChildCount());
        assertEquals(toolbarView, toolbarContainer.getChildAt(0));

        mModel.set(BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT, 200);
        assertEquals(200, mView.findViewById(R.id.keyboard_curtain).getLayoutParams().height);

        mModel.set(BottomSheetProperties.CONTAINER_HEIGHT, 400);
        assertEquals(400, contentContainer.getLayoutParams().height);

        mModel.set(BottomSheetProperties.SHEET_WIDTH_PX, 500);
        assertEquals(500, mView.getLayoutParams().width);

        mModel.set(BottomSheetProperties.ACCESSIBILITY_PANE_TITLE, "Sheet Title");
        assertEquals("Sheet Title", ViewCompat.getAccessibilityPaneTitle(mView));

        mModel.set(BottomSheetProperties.SHEET_TRANSLATION_Y, 150f);
        assertEquals(150f, mView.getTranslationY(), 0f);

        mModel.set(BottomSheetProperties.SHEET_TRANSLATION_X, 75f);
        assertEquals(75f, mView.getTranslationX(), 0f);

        View handlebar = mView.findViewById(R.id.handlebar);
        mModel.set(BottomSheetProperties.HANDLEBAR_VISIBLE, true);
        assertEquals(View.VISIBLE, handlebar.getVisibility());

        mModel.set(BottomSheetProperties.CONTENT_TOP_MARGIN, 32);
        MarginLayoutParams params = (MarginLayoutParams) contentContainer.getLayoutParams();
        assertEquals(32, params.topMargin);

        mModel.set(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT, 500);
        assertEquals(500, background.getBottom());

        mModel.set(BottomSheetProperties.SHEET_FOCUSABLE, true);
        assertTrue(mView.isFocusable());

        mModel.set(BottomSheetProperties.CONTENT_BOTTOM_PADDING, 24);
        assertEquals(24, contentContainer.getPaddingBottom());

        mModel.set(BottomSheetProperties.BACKGROUND_HEIGHT, 600);
        assertEquals(600, background.getLayoutParams().height);

        mModel.set(BottomSheetProperties.TOUCH_HANDLER, mTouchHandler);
        mView.onTouchEvent(event);
        verify(mTouchHandler).onTouchEvent(event);

        mModel.set(BottomSheetProperties.HANDLEBAR_CLICK_LISTENER, mClickListener);
        handlebar.performClick();
        verify(mClickListener).onClick(any());

        mModel.set(BottomSheetProperties.TOOLBAR_SIZE_CHANGED_CALLBACK, mCallback);
        toolbarContainer.layout(0, 0, 100, 50);
        verify(mCallback).run();

        mView.setAccessibilityDelegate(mAccessibilityDelegate);

        mModel.set(BottomSheetProperties.CURRENT_SHEET_STATE, SheetState.HIDDEN);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mAccessibilityDelegate, never())
                .performAccessibilityAction(
                        mView, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);

        mModel.set(BottomSheetProperties.CURRENT_SHEET_STATE, SheetState.PEEK);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mAccessibilityDelegate)
                .performAccessibilityAction(
                        mView, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);

        assertEquals(BottomSheetProperties.ALL_KEYS.length, mModel.getAllSetProperties().size());
    }
}
