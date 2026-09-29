// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.verify;

import android.graphics.Color;
import android.view.View;
import android.view.View.OnClickListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.SheetLayoutMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.TouchHandler;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public class BottomSheetViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetView mView;
    @Mock private TouchHandler mTouchHandler;
    @Mock private OnClickListener mClickListener;
    @Mock private Runnable mCallback;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(BottomSheetProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, BottomSheetViewBinder::bind);
    }

    @Test
    public void testSheetLayoutMode() {
        mModel.set(BottomSheetProperties.SHEET_LAYOUT_MODE, SheetLayoutMode.DESKTOP_POPUP);
        verify(mView).setSheetLayoutMode(SheetLayoutMode.DESKTOP_POPUP);
    }

    @Test
    public void testGlowSpec() {
        GlowSpec spec = new GlowSpec(Color.RED, GlowSpec.ShadowSize.LONG);
        mModel.set(BottomSheetProperties.GLOW_SPEC, spec);
        verify(mView).setGlowSpec(spec);
    }

    @Test
    public void testBackgroundColor() {
        mModel.set(BottomSheetProperties.BACKGROUND_COLOR, Color.GREEN);
        verify(mView).setSheetBackgroundColor(Color.GREEN);
    }

    @Test
    public void testCloseButtonVisibility() {
        mModel.set(BottomSheetProperties.CLOSE_BUTTON_VISIBILITY, true);
        verify(mView).setCloseButtonVisible(true);
    }

    @Test
    public void testCloseButtonClickListener() {
        OnClickListener listener = mock(OnClickListener.class);
        mModel.set(BottomSheetProperties.CLOSE_BUTTON_CLICK_LISTENER, listener);
        verify(mView).setCloseButtonClickListener(listener);
    }

    @Test
    public void testContainerTouchEnabled() {
        mModel.set(BottomSheetProperties.CONTAINER_TOUCH_ENABLED, false);
        verify(mView).setContainerTouchEnabled(false);
    }

    @Test
    public void testContentView() {
        View contentView = mock(View.class);
        mModel.set(BottomSheetProperties.CONTENT_VIEW, contentView);
        verify(mView).setContentView(contentView);
    }

    @Test
    public void testToolbarView() {
        View toolbarView = mock(View.class);
        mModel.set(BottomSheetProperties.TOOLBAR_VIEW, toolbarView);
        verify(mView).setToolbarView(toolbarView);
    }

    @Test
    public void testKeyboardCurtainHeight() {
        mModel.set(BottomSheetProperties.KEYBOARD_CURTAIN_HEIGHT, 200);
        verify(mView).setKeyboardCurtainHeight(200);
    }

    @Test
    public void testContainerHeight() {
        mModel.set(BottomSheetProperties.CONTAINER_HEIGHT, 400);
        verify(mView).setContainerHeight(400);
    }

    @Test
    public void testSheetWidth() {
        mModel.set(BottomSheetProperties.SHEET_WIDTH_PX, 500);
        verify(mView).setSheetWidth(500);
    }

    @Test
    public void testAccessibilityPaneTitle() {
        mModel.set(BottomSheetProperties.ACCESSIBILITY_PANE_TITLE, "Sheet Title");
        verify(mView).setSheetAccessibilityPaneTitle("Sheet Title");
    }

    @Test
    public void testSheetTranslationY() {
        mModel.set(BottomSheetProperties.SHEET_TRANSLATION_Y, 150f);
        verify(mView).setSheetTranslationY(150f);
    }

    @Test
    public void testSheetTranslationX() {
        mModel.set(BottomSheetProperties.SHEET_TRANSLATION_X, 75f);
        verify(mView).setSheetTranslationX(75f);
    }

    @Test
    public void testHandlebarVisible() {
        mModel.set(BottomSheetProperties.HANDLEBAR_VISIBLE, true);
        verify(mView).setHandlebarVisible(true);
    }

    @Test
    public void testContentTopMargin() {
        mModel.set(BottomSheetProperties.CONTENT_TOP_MARGIN, 32);
        verify(mView).setContentTopMargin(32);
    }

    @Test
    public void testVisibleBackgroundHeight() {
        mModel.set(BottomSheetProperties.VISIBLE_BACKGROUND_HEIGHT, 500);
        verify(mView).setVisibleBackgroundHeight(500);
    }

    @Test
    public void testSheetFocusable() {
        mModel.set(BottomSheetProperties.SHEET_FOCUSABLE, true);
        verify(mView).setSheetFocusable(true);
    }

    @Test
    public void testContentBottomPadding() {
        mModel.set(BottomSheetProperties.CONTENT_BOTTOM_PADDING, 24);
        verify(mView).setContentContainerPaddingBottom(24);
    }

    @Test
    public void testBackgroundHeight() {
        mModel.set(BottomSheetProperties.BACKGROUND_HEIGHT, 600);
        verify(mView).updateBackgroundHeight(600);
    }

    @Test
    public void testTouchHandler() {
        mModel.set(BottomSheetProperties.TOUCH_HANDLER, mTouchHandler);
        verify(mView).setTouchHandler(mTouchHandler);
    }

    @Test
    public void testHandlebarClickListener() {
        mModel.set(BottomSheetProperties.HANDLEBAR_CLICK_LISTENER, mClickListener);
        verify(mView).setHandlebarClickListener(mClickListener);
    }

    @Test
    public void testToolbarSizeChangedCallback() {
        mModel.set(BottomSheetProperties.TOOLBAR_SIZE_CHANGED_CALLBACK, mCallback);
        verify(mView).setToolbarSizeChangedCallback(mCallback);
    }

    @Test
    public void testCurrentSheetState() {
        mModel.set(BottomSheetProperties.CURRENT_SHEET_STATE, SheetState.PEEK);
        verify(mView).sendPaneChangeAccessibilityEvent(true);

        mModel.set(BottomSheetProperties.CURRENT_SHEET_STATE, SheetState.HIDDEN);
        verify(mView).sendPaneChangeAccessibilityEvent(false);
    }

    @Test
    public void testAllKeysAreBound() {
        for (PropertyKey key : BottomSheetProperties.ALL_KEYS) {
            clearInvocations(mView);
            BottomSheetViewBinder.bind(mModel, mView, key);
            assertFalse(
                    "Every key in BottomSheetProperties.ALL_KEYS must be handled by"
                            + " BottomSheetViewBinder: "
                            + key,
                    mockingDetails(mView).getInvocations().isEmpty());
        }
    }
}
