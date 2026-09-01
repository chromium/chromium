// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.mockito.Mockito.mock;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.SheetLayoutMode;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetView mView;
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
    public void testContainerZ() {
        mModel.set(BottomSheetProperties.CONTAINER_Z, 12f);
        verify(mView).setContainerZ(12f);
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
    public void testFallbackShadowVisibility() {
        mModel.set(BottomSheetProperties.FALLBACK_SHADOW_VISIBILITY, true);
        verify(mView).setFallbackShadowVisible(true);
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
}
