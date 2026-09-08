// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;

/** Unit tests for {@link BottomSheetUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mBottomSheetContent;

    @Test
    public void testIsContentActingAsBrowserControls_NullController() {
        assertFalse(BottomSheetUtils.isContentActingAsBrowserControls(null, true));
    }

    @Test
    public void testIsContentActingAsBrowserControls_FeatureDisabled() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);
        when(mBottomSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mBottomSheetController.isFullWidth()).thenReturn(true);

        assertFalse(
                BottomSheetUtils.isContentActingAsBrowserControls(
                        mBottomSheetController, /* isBottomSheetAsBrowserControlsEnabled= */ false));
    }

    @Test
    public void testIsContentActingAsBrowserControls_NullContent() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(null);

        assertFalse(
                BottomSheetUtils.isContentActingAsBrowserControls(
                        mBottomSheetController, /* isBottomSheetAsBrowserControlsEnabled= */ true));
    }

    @Test
    public void testIsContentActingAsBrowserControls_DoesNotActAsBrowserControls() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);
        when(mBottomSheetContent.actsAsBrowserControls()).thenReturn(false);
        when(mBottomSheetController.isFullWidth()).thenReturn(true);

        assertFalse(
                BottomSheetUtils.isContentActingAsBrowserControls(
                        mBottomSheetController, /* isBottomSheetAsBrowserControlsEnabled= */ true));
    }

    @Test
    public void testIsContentActingAsBrowserControls_NotFullWidth() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);
        when(mBottomSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mBottomSheetController.isFullWidth()).thenReturn(false);

        assertFalse(
                BottomSheetUtils.isContentActingAsBrowserControls(
                        mBottomSheetController, /* isBottomSheetAsBrowserControlsEnabled= */ true));
    }

    @Test
    public void testIsContentActingAsBrowserControls_Success() {
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);
        when(mBottomSheetContent.actsAsBrowserControls()).thenReturn(true);
        when(mBottomSheetController.isFullWidth()).thenReturn(true);

        assertTrue(
                BottomSheetUtils.isContentActingAsBrowserControls(
                        mBottomSheetController, /* isBottomSheetAsBrowserControlsEnabled= */ true));
    }

    @Test
    public void testIsSheetNonModal_NullContent() {
        assertFalse(BottomSheetUtils.isSheetNonModal(null));
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_TYPES)
    public void testIsSheetNonModal_TypesEnabled_Modal() {
        BottomSheetType modalType = new BottomSheetType.Builder().setModal(true).build();
        when(mBottomSheetContent.getSheetType()).thenReturn(modalType);

        assertFalse(BottomSheetUtils.isSheetNonModal(mBottomSheetContent));
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_TYPES)
    public void testIsSheetNonModal_TypesEnabled_NonModal() {
        BottomSheetType nonModalType = new BottomSheetType.Builder().setModal(false).build();
        when(mBottomSheetContent.getSheetType()).thenReturn(nonModalType);

        assertTrue(BottomSheetUtils.isSheetNonModal(mBottomSheetContent));
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_TYPES)
    public void testIsSheetNonModal_TypesEnabled_IgnoresCustomScrimLifecycle() {
        // When types feature is enabled, modal=true should return false even if hasCustomScrimLifecycle is true.
        BottomSheetType modalType = new BottomSheetType.Builder().setModal(true).build();
        when(mBottomSheetContent.getSheetType()).thenReturn(modalType);
        when(mBottomSheetContent.hasCustomScrimLifecycle()).thenReturn(true);

        assertFalse(BottomSheetUtils.isSheetNonModal(mBottomSheetContent));

        // When types feature is enabled, modal=false should return true even if hasCustomScrimLifecycle is false.
        BottomSheetType nonModalType = new BottomSheetType.Builder().setModal(false).build();
        when(mBottomSheetContent.getSheetType()).thenReturn(nonModalType);
        when(mBottomSheetContent.hasCustomScrimLifecycle()).thenReturn(false);

        assertTrue(BottomSheetUtils.isSheetNonModal(mBottomSheetContent));
    }

    @Test
    @DisableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_TYPES)
    public void testIsSheetNonModal_TypesDisabled_CustomScrimLifecycleTrue() {
        when(mBottomSheetContent.hasCustomScrimLifecycle()).thenReturn(true);

        assertTrue(BottomSheetUtils.isSheetNonModal(mBottomSheetContent));
    }

    @Test
    @DisableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_TYPES)
    public void testIsSheetNonModal_TypesDisabled_CustomScrimLifecycleFalse() {
        when(mBottomSheetContent.hasCustomScrimLifecycle()).thenReturn(false);

        assertFalse(BottomSheetUtils.isSheetNonModal(mBottomSheetContent));
    }
}
