// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.insets.InsetObserver;

import java.lang.ref.WeakReference;

/** Unit tests for {@link OmniboxSuggestionsDropdownEmbedderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownEmbedderImplTest {
    private static final int ANCHOR_WIDTH = 600;
    private static final int ANCHOR_HEIGHT = 80;
    private static final int ANCHOR_TOP = 31;
    private static final int TABLET_OVERLAP = 2;

    private static final int INTERMEDIATE_VIEW_TOP = 75;

    private static final int ALIGNMENT_WIDTH = 400;
    // Sentinel value for mistaken use of alignment view top instead of left. If you see a 43, it's
    // probably because you used position[1] instead of position[0].
    private static final int ALIGNMENT_TOP = 43;
    private static final int ALIGNMENT_LEFT = 40;
    private static final int ALIGNMENT_HEIGHT = 45;

    // Sentinel value for mistaken use of pixels. OmniboxSuggestionsDropdownEmbedderImpl should
    // operate solely in terms of dp so values that are 10x their correct size are probably
    // being inadvertently converted to px.
    private static final float DIP_SCALE = 10.0f;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock WindowAndroid mWindowAndroid;
    private @Mock ViewTreeObserver mViewTreeObserver;
    private @Mock ViewGroup mContentView;
    private @Mock ViewGroup mAnchorView;
    private @Mock ViewGroup mIntermediateView;
    private @Mock View mHorizontalAlignmentView;
    private @Mock DisplayAndroid mDisplay;
    private @Mock InsetObserver mInsetObserver;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock TopInsetProvider mTopInsetProvider;

    private OmniboxSuggestionsDropdownEmbedderImpl mImpl;
    private WeakReference<Context> mContextWeakRef;
    private int mBottomWindowPadding;
    private @ControlsPosition int mControlsPosition = ControlsPosition.TOP;
    private final SettableNonNullObservableSupplier<Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);

    @Before
    public void setUp() {
        mContextWeakRef = new WeakReference<>(ContextUtils.getApplicationContext());
        doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();
        doReturn(mContextWeakRef).when(mWindowAndroid).getContext();
        doReturn(mContextWeakRef.get()).when(mAnchorView).getContext();
        doReturn(mViewTreeObserver).when(mAnchorView).getViewTreeObserver();
        doReturn(mContentView).when(mAnchorView).getRootView();
        doReturn(mContentView).when(mContentView).findViewById(android.R.id.content);
        doReturn(mContentView).when(mAnchorView).getParent();
        doReturn(Integer.MAX_VALUE).when(mContentView).getMeasuredHeight();
        doReturn(ANCHOR_WIDTH).when(mAnchorView).getMeasuredWidth();
        doReturn(ALIGNMENT_WIDTH).when(mHorizontalAlignmentView).getMeasuredWidth();
        doReturn(ANCHOR_HEIGHT).when(mAnchorView).getMeasuredHeight();
        doReturn(ANCHOR_TOP).when(mAnchorView).getTop();
        doReturn(ALIGNMENT_TOP).when(mHorizontalAlignmentView).getTop();
        doReturn(ALIGNMENT_LEFT).when(mHorizontalAlignmentView).getLeft();
        doReturn(ALIGNMENT_HEIGHT).when(mHorizontalAlignmentView).getMeasuredHeight();
        doReturn(mDisplay).when(mWindowAndroid).getDisplay();
        doReturn(DIP_SCALE).when(mDisplay).getDipScale();
        doReturn((int) (getConfiguration().screenHeightDp * DIP_SCALE))
                .when(mDisplay)
                .getDisplayHeight();
        mImpl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mWindowAndroid,
                        mAnchorView,
                        mHorizontalAlignmentView,
                        false,
                        mContentView,
                        () -> mControlsPosition,
                        () -> 0,
                        () -> mBottomWindowPadding,
                        mFuseboxStateSupplier,
                        mLocationBarDataProvider,
                        mTopInsetProvider);
    }

    @Test
    public void testWindowAttachment() {
        // TopInsetProvider observer should be added during construction.
        verify(mTopInsetProvider).addObserver(any(TopInsetProvider.Observer.class));

        verify(mAnchorView, never()).addOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView, never()).addOnLayoutChangeListener(mImpl);
        verify(mAnchorView, never()).getViewTreeObserver();

        mImpl.onAttachedToWindow();

        verify(mAnchorView).addOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView).addOnLayoutChangeListener(mImpl);
        verify(mViewTreeObserver).addOnGlobalLayoutListener(mImpl);

        mImpl.onDetachedFromWindow();
        verify(mAnchorView).removeOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView).removeOnLayoutChangeListener(mImpl);
        verify(mViewTreeObserver).removeOnGlobalLayoutListener(mImpl);
    }

    @Test
    public void testRecalculateOmniboxAlignment_phone() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    public void testRecalculateOmniboxAlignment_bottomWindowPadding() {
        mBottomWindowPadding = 40;
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP) + 40,
                        0,
                        0,
                        0,
                        40),
                alignment);

        mBottomWindowPadding = 0;
        mImpl.recalculateOmniboxAlignment();
        alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    public void testRecalculateOmniboxAlignment_definedBaseChromeLayout() {
        // Add an intermediate view between the anchorView and contentView
        doReturn(mIntermediateView).when(mAnchorView).getParent();
        doReturn(mContentView).when(mIntermediateView).getParent();
        doReturn(INTERMEDIATE_VIEW_TOP).when(mIntermediateView).getTop();
        doReturn(Integer.MAX_VALUE).when(mIntermediateView).getMeasuredHeight();

        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();

        OmniboxSuggestionsDropdownEmbedderImpl impl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mWindowAndroid,
                        mAnchorView,
                        mHorizontalAlignmentView,
                        false,
                        mIntermediateView,
                        () -> mControlsPosition,
                        () -> 0,
                        () -> 0,
                        mFuseboxStateSupplier,
                        mLocationBarDataProvider,
                        mTopInsetProvider);
        impl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = impl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    public void testRecalculateOmniboxAlignment_contentViewPadding() {
        doReturn(13).when(mContentView).getPaddingTop();
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP - 13,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP - 13),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    public void testRecalculateOmniboxAlignment_phoneRevampEnabled() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    public void testRecalculateOmniboxAlignment_bottomControlsPosition() {
        mControlsPosition = ControlsPosition.BOTTOM;
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0, 0, ANCHOR_WIDTH, getExpectedHeight(0) - ANCHOR_HEIGHT, 0, 0, 0, 0),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_tabletToPhoneSwitch() {
        int sideSpacing = OmniboxResourceProvider.getDropdownSideSpacing(mContextWeakRef.get());
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        assertTrue(mImpl.isTablet());
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        int expectedTop = ANCHOR_HEIGHT + ANCHOR_TOP - TABLET_OVERLAP;
        assertEquals(
                new OmniboxAlignment(
                        ALIGNMENT_LEFT - sideSpacing,
                        expectedTop,
                        ALIGNMENT_WIDTH + 2 * sideSpacing,
                        getExpectedHeight(expectedTop),
                        0,
                        0,
                        0,
                        0),
                alignment);

        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1;
        mImpl.onConfigurationChanged(newConfig);
        assertFalse(mImpl.isTablet());
        OmniboxAlignment newAlignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP),
                        0,
                        0,
                        0,
                        0),
                newAlignment);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_phoneToTabletSwitch() {
        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1;
        mImpl.onConfigurationChanged(newConfig);
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        assertFalse(mImpl.isTablet());
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0,
                        ANCHOR_HEIGHT + ANCHOR_TOP,
                        ANCHOR_WIDTH,
                        getExpectedHeight(ANCHOR_HEIGHT + ANCHOR_TOP),
                        0,
                        0,
                        0,
                        0),
                alignment);

        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        int sideSpacing = OmniboxResourceProvider.getDropdownSideSpacing(mContextWeakRef.get());
        mImpl.onConfigurationChanged(newConfig);
        assertTrue(mImpl.isTablet());
        OmniboxAlignment newAlignment = mImpl.getCurrentAlignment();
        int expectedTop = ANCHOR_HEIGHT + ANCHOR_TOP - TABLET_OVERLAP;
        assertEquals(
                new OmniboxAlignment(
                        ALIGNMENT_LEFT - sideSpacing,
                        expectedTop,
                        ALIGNMENT_WIDTH + 2 * sideSpacing,
                        getExpectedHeight(expectedTop),
                        0,
                        0,
                        0,
                        0),
                newAlignment);
    }

    @Test
    @Config(qualifiers = "sw400dp")
    public void testAdaptToNarrowWindows_widePhoneScreen() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        assertFalse(mImpl.isTablet());

        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        mImpl.onConfigurationChanged(newConfig);
        assertFalse(mImpl.isTablet());
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_tablet_ltr() {
        int sideSpacing = OmniboxResourceProvider.getDropdownSideSpacing(mContextWeakRef.get());
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        int expectedTop = ANCHOR_HEIGHT + ANCHOR_TOP - TABLET_OVERLAP;
        assertEquals(
                new OmniboxAlignment(
                        ALIGNMENT_LEFT - sideSpacing,
                        expectedTop,
                        ALIGNMENT_WIDTH + 2 * sideSpacing,
                        getExpectedHeight(expectedTop),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_tablet_fusebox() {
        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        int expectedTop = ALIGNMENT_HEIGHT + ANCHOR_TOP + ALIGNMENT_TOP;
        assertEquals(
                new OmniboxAlignment(
                        ALIGNMENT_LEFT,
                        expectedTop,
                        ALIGNMENT_WIDTH,
                        getExpectedHeight(expectedTop),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldrtl-sw600dp-h100dp")
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_tablet_rtl() {
        int sideSpacing = OmniboxResourceProvider.getDropdownSideSpacing(mContextWeakRef.get());
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mAnchorView).getLayoutDirection();
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        int expectedWidth = ALIGNMENT_WIDTH + 2 * sideSpacing;
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        int expectedTop = ANCHOR_HEIGHT + ANCHOR_TOP - TABLET_OVERLAP;
        assertEquals(
                new OmniboxAlignment(
                        -(ANCHOR_WIDTH - expectedWidth - ALIGNMENT_LEFT + sideSpacing),
                        expectedTop,
                        expectedWidth,
                        getExpectedHeight(expectedTop),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_tablet_mainSpaceAboveWindowBottom() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        doReturn((int) (DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP * DIP_SCALE))
                .when(mDisplay)
                .getDisplayHeight();

        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        newConfig.screenHeightDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
        int sideSpacing = OmniboxResourceProvider.getDropdownSideSpacing(mContextWeakRef.get());
        mImpl.onConfigurationChanged(newConfig);

        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        int top = ANCHOR_HEIGHT + ANCHOR_TOP - TABLET_OVERLAP;
        assertEquals(
                new OmniboxAlignment(
                        ALIGNMENT_LEFT - sideSpacing,
                        top,
                        ALIGNMENT_WIDTH + 2 * sideSpacing,
                        getExpectedHeight(top),
                        0,
                        0,
                        0,
                        0),
                alignment);
    }

    private int getExpectedHeight(int top) {
        int minHeightAboveWindowBottom =
                mContextWeakRef
                        .get()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_min_space_above_window_bottom);
        return (int) (getConfiguration().screenHeightDp * DIP_SCALE - top)
                - minHeightAboveWindowBottom;
    }

    private Configuration getConfiguration() {
        return mContextWeakRef.get().getResources().getConfiguration();
    }

    @Test
    public void testOnToEdgeChange() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();

        // With controls at top, paddingTop should remain 0.
        mControlsPosition = ControlsPosition.TOP;
        mImpl.onToEdgeChange(
                /* systemTopInset= */ 100, /* consumeTopInset= */ true, LayoutType.BROWSING);
        assertEquals(0, mImpl.getCurrentAlignment().paddingTop);

        // With controls at bottom and consumeTopInset=true, paddingTop should match systemTopInset.
        mControlsPosition = ControlsPosition.BOTTOM;
        mImpl.onToEdgeChange(
                /* systemTopInset= */ 100, /* consumeTopInset= */ true, LayoutType.BROWSING);
        assertEquals(100, mImpl.getCurrentAlignment().paddingTop);

        // With consumeTopInset=false, paddingTop should reset to 0.
        mImpl.onToEdgeChange(
                /* systemTopInset= */ 100, /* consumeTopInset= */ false, LayoutType.BROWSING);
        assertEquals(0, mImpl.getCurrentAlignment().paddingTop);

        // paddingTop should update from non-zero to zero when systemTopInset changes.
        mImpl.onToEdgeChange(
                /* systemTopInset= */ 50, /* consumeTopInset= */ true, LayoutType.BROWSING);
        assertEquals(50, mImpl.getCurrentAlignment().paddingTop);

        mImpl.onToEdgeChange(
                /* systemTopInset= */ 0, /* consumeTopInset= */ true, LayoutType.BROWSING);
        assertEquals(0, mImpl.getCurrentAlignment().paddingTop);
    }

    @Test
    public void testDestroy_removesTopInsetObserver() {
        mImpl.destroy();
        verify(mTopInsetProvider).removeObserver(any(TopInsetProvider.Observer.class));
    }
}
