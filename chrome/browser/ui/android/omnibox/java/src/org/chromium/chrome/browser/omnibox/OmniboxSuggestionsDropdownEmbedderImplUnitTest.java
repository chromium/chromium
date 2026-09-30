// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.os.Build;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.WindowInsets;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.insets.InsetObserver;

import java.lang.ref.WeakReference;

/** Unit tests for {@link OmniboxSuggestionsDropdownEmbedderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownEmbedderImplUnitTest {
    private static final int ANCHOR_WIDTH = 600;
    private static final int ANCHOR_HEIGHT = 80;
    private static final int ANCHOR_TOP = 31;
    private static final int TABLET_OVERLAP = 2;

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

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DisplayAndroid mDisplay;
    @Mock private InsetObserver mInsetObserver;
    @Mock private TopInsetProvider mTopInsetProvider;
    @Mock private Callback<OmniboxAlignment> mAlignmentChanged;

    private FrameLayout mContentView;
    private FrameLayout mAnchorView;
    private FrameLayout mIntermediateView;
    private View mHorizontalAlignmentView;
    private OmniboxResourceProvider mResourceProvider;
    private OmniboxSuggestionsDropdownEmbedderImpl mImpl;
    private Context mContext;
    private int mBottomWindowPadding;
    private @ControlsPosition int mControlsPosition = ControlsPosition.TOP;
    private final SettableNonNullObservableSupplier<Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final SettableNonNullObservableSupplier<Integer> mFuseboxLayoutModeSupplier =
            ObservableSuppliers.createNonNull(FuseboxLayoutMode.TOOLBAR);
    private boolean mIsFullWidthExpansionAllowed = true;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.getApplicationInfo().flags |= ApplicationInfo.FLAG_SUPPORTS_RTL;
        mContext = activity;
        mResourceProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        lenient().doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();
        lenient().doReturn(new WeakReference<>(mContext)).when(mWindowAndroid).getContext();

        mContentView = new FrameLayout(mContext);
        activity.setContentView(mContentView);
        mContentView.setId(android.R.id.content);
        setBounds(mContentView, 0, 0, ANCHOR_WIDTH, View.MEASURED_SIZE_MASK);

        mAnchorView = new FrameLayout(mContext);
        setBounds(mAnchorView, 0, ANCHOR_TOP, ANCHOR_WIDTH, ANCHOR_HEIGHT);
        mContentView.addView(mAnchorView);

        mHorizontalAlignmentView = new View(mContext);
        setBounds(
                mHorizontalAlignmentView,
                ALIGNMENT_LEFT,
                ALIGNMENT_TOP,
                ALIGNMENT_WIDTH,
                ALIGNMENT_HEIGHT);
        mAnchorView.addView(mHorizontalAlignmentView);

        mIntermediateView = new FrameLayout(mContext);
        setBounds(mIntermediateView, 0, 0, ANCHOR_WIDTH, View.MEASURED_SIZE_MASK);

        lenient().doReturn(mDisplay).when(mWindowAndroid).getDisplay();
        lenient().doReturn(DIP_SCALE).when(mDisplay).getDipScale();
        lenient()
                .doReturn((int) (getConfiguration().screenHeightDp * DIP_SCALE))
                .when(mDisplay)
                .getDisplayHeight();
        mImpl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mResourceProvider,
                        mWindowAndroid,
                        mAnchorView,
                        mHorizontalAlignmentView,
                        mHorizontalAlignmentView::getMeasuredWidth,
                        () -> 0,
                        false,
                        mContentView,
                        () -> mControlsPosition,
                        () -> 0,
                        () -> mBottomWindowPadding,
                        mFuseboxStateSupplier,
                        mFuseboxLayoutModeSupplier,
                        mTopInsetProvider,
                        () -> mIsFullWidthExpansionAllowed);
    }

    private static void setBounds(View view, int left, int top, int width, int height) {
        view.setLayoutParams(new FrameLayout.LayoutParams(width, height));
        view.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        view.setLeft(left);
        view.setTop(top);
        view.setRight(left + width);
        view.setBottom(top + height);
    }

    @Test
    public void testWindowAttachment() {
        // TopInsetProvider observer should be added during construction.
        verify(mTopInsetProvider).addObserver(any(TopInsetProvider.Observer.class));

        mImpl.addAlignmentObserver(mAlignmentChanged);
        clearInvocations(mAlignmentChanged);

        // Before attachment, layout changes do not trigger alignment recalculation.
        mAnchorView.layout(0, ANCHOR_TOP + 1, ANCHOR_WIDTH, ANCHOR_TOP + 1 + ANCHOR_HEIGHT);
        verify(mAlignmentChanged, never()).onResult(any());

        mImpl.onAttachedToWindow();
        clearInvocations(mAlignmentChanged);

        // While attached, layout changes recalculate alignment.
        mAnchorView.layout(0, ANCHOR_TOP + 2, ANCHOR_WIDTH, ANCHOR_TOP + 2 + ANCHOR_HEIGHT);
        verify(mAlignmentChanged).onResult(any());
        clearInvocations(mAlignmentChanged);

        // After detachment, layout changes no longer recalculate alignment.
        mImpl.onDetachedFromWindow();
        mAnchorView.layout(0, ANCHOR_TOP + 3, ANCHOR_WIDTH, ANCHOR_TOP + 3 + ANCHOR_HEIGHT);
        verify(mAlignmentChanged, never()).onResult(any());
    }

    @Test
    public void testPositionInWindow() {
        mImpl.onAttachedToWindow();
        // Prime initial vertical and horizontal window offsets.
        mImpl.onGlobalLayout();
        mImpl.onGlobalLayout();
        mImpl.addAlignmentObserver(mAlignmentChanged);
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

        setBounds(mAnchorView, 0, ANCHOR_TOP, ANCHOR_WIDTH - 1, ANCHOR_HEIGHT);
        mImpl.onGlobalLayout();

        mHorizontalAlignmentView.setLeft(ALIGNMENT_LEFT + 1);

        mImpl.onGlobalLayout();
        verify(mAlignmentChanged).onResult(any(OmniboxAlignment.class));
    }

    @Test
    public void testRecalculateOmniboxAlignment_phone() {
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
        mContentView.removeView(mAnchorView);
        mIntermediateView.addView(mAnchorView);
        mContentView.addView(mIntermediateView);

        OmniboxSuggestionsDropdownEmbedderImpl impl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mResourceProvider,
                        mWindowAndroid,
                        mAnchorView,
                        mHorizontalAlignmentView,
                        () -> 0,
                        () -> 0,
                        false,
                        mIntermediateView,
                        () -> mControlsPosition,
                        () -> 0,
                        () -> 0,
                        mFuseboxStateSupplier,
                        mFuseboxLayoutModeSupplier,
                        mTopInsetProvider,
                        () -> true);
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
        mContentView.setPadding(0, 13, 0, 0);
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
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(
                        0, 0, ANCHOR_WIDTH, getExpectedHeight(0) - ANCHOR_HEIGHT, 0, 0, 0, 0),
                alignment);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testRecalculateOmniboxAlignment_bottomControlsWithKeyboardSubtractsNavBarHeight() {
        int navBarHeight = 100;
        int keyboardHeight = 500;
        mControlsPosition = ControlsPosition.BOTTOM;

        WindowInsets windowInsets =
                new WindowInsets.Builder()
                        .setInsets(
                                WindowInsets.Type.navigationBars(),
                                Insets.of(0, 0, 0, navBarHeight))
                        .build();
        FrameLayout contentViewWithInsets =
                new FrameLayout(mContext) {
                    @Override
                    public WindowInsets getRootWindowInsets() {
                        return windowInsets;
                    }
                };
        setBounds(contentViewWithInsets, 0, 0, ANCHOR_WIDTH, View.MEASURED_SIZE_MASK);

        OmniboxSuggestionsDropdownEmbedderImpl impl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mResourceProvider,
                        mWindowAndroid,
                        mAnchorView,
                        mHorizontalAlignmentView,
                        () -> 0,
                        () -> 0,
                        false,
                        contentViewWithInsets,
                        () -> mControlsPosition,
                        () -> keyboardHeight,
                        () -> mBottomWindowPadding,
                        mFuseboxStateSupplier,
                        mFuseboxLayoutModeSupplier,
                        mTopInsetProvider,
                        () -> true);

        impl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = impl.getCurrentAlignment();

        int windowHeight = (int) (getConfiguration().screenHeightDp * DIP_SCALE);
        int minSpaceAboveWindowBottom =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_min_space_above_window_bottom);
        int windowSpace =
                Math.min(windowHeight - keyboardHeight, windowHeight - minSpaceAboveWindowBottom);
        int contentSpace = Integer.MAX_VALUE - keyboardHeight;
        int expectedHeight = Math.min(windowSpace, contentSpace) - ANCHOR_HEIGHT - navBarHeight;

        assertEquals(
                new OmniboxAlignment(0, 0, ANCHOR_WIDTH, expectedHeight, 0, 0, 0, 0), alignment);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testRecalculateOmniboxAlignment_bottomControlsNoKeyboardDoesNotSubtractNavBar() {
        // When keyboard is hidden (keyboardHeight == 0), nav bar should NOT be subtracted.
        int navBarHeight = 100;
        mControlsPosition = ControlsPosition.BOTTOM;

        WindowInsets windowInsets =
                new WindowInsets.Builder()
                        .setInsets(
                                WindowInsets.Type.navigationBars(),
                                Insets.of(0, 0, 0, navBarHeight))
                        .build();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        // No navBarHeight subtracted since keyboardHeight == 0.
        assertEquals(
                new OmniboxAlignment(
                        0, 0, ANCHOR_WIDTH, getExpectedHeight(0) - ANCHOR_HEIGHT, 0, 0, 0, 0),
                alignment);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testRecalculateOmniboxAlignment_topControlsDoesNotSubtractNavBarHeight() {
        int navBarHeight = 100;
        mControlsPosition = ControlsPosition.TOP;

        WindowInsets windowInsets =
                new WindowInsets.Builder()
                        .setInsets(
                                WindowInsets.Type.navigationBars(),
                                Insets.of(0, 0, 0, navBarHeight))
                        .build();
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
    @Config(qualifiers = "ldltr-sw600dp")
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_tabletToPhoneSwitch() {
        int sideSpacing = mResourceProvider.getDropdownSideSpacing();
        assertTrue(mImpl.isWideWindow());
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
        assertFalse(mImpl.isWideWindow());
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
        assertFalse(mImpl.isWideWindow());
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
        int sideSpacing = mResourceProvider.getDropdownSideSpacing();
        mImpl.onConfigurationChanged(newConfig);
        assertTrue(mImpl.isWideWindow());
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
        assertFalse(mImpl.isWideWindow());

        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        mImpl.onConfigurationChanged(newConfig);
        assertFalse(mImpl.isWideWindow());
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    public void testRecalculateOmniboxAlignment_tablet_ltr() {
        int sideSpacing = mResourceProvider.getDropdownSideSpacing();
        setBounds(mHorizontalAlignmentView, ALIGNMENT_LEFT, 60, ALIGNMENT_WIDTH, ALIGNMENT_HEIGHT);
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
    @Config(qualifiers = "ldltr-sw600dp")
    public void testRecalculateOmniboxAlignment_tablet_popoverSuppliers() {
        int targetWidth = 504;
        int leftOffset = -40;
        OmniboxSuggestionsDropdownEmbedderImpl impl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mResourceProvider,
                        mWindowAndroid,
                        mAnchorView,
                        mHorizontalAlignmentView,
                        () -> targetWidth,
                        () -> leftOffset,
                        false,
                        mContentView,
                        () -> mControlsPosition,
                        () -> 0,
                        () -> mBottomWindowPadding,
                        mFuseboxStateSupplier,
                        mFuseboxLayoutModeSupplier,
                        mTopInsetProvider,
                        () -> true);

        setBounds(mHorizontalAlignmentView, ALIGNMENT_LEFT, 60, ALIGNMENT_WIDTH, ALIGNMENT_HEIGHT);
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);

        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        impl.onConfigurationChanged(newConfig);

        OmniboxAlignment alignment = impl.getCurrentAlignment();

        assertEquals(
                new OmniboxAlignment(
                        ALIGNMENT_LEFT + leftOffset,
                        ANCHOR_TOP,
                        targetWidth,
                        getExpectedHeight(ANCHOR_TOP),
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
        int sideSpacing = mResourceProvider.getDropdownSideSpacing();
        mAnchorView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        setBounds(mHorizontalAlignmentView, ALIGNMENT_LEFT, 60, ALIGNMENT_WIDTH, ALIGNMENT_HEIGHT);
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
        setBounds(mHorizontalAlignmentView, ALIGNMENT_LEFT, 60, ALIGNMENT_WIDTH, ALIGNMENT_HEIGHT);
        doReturn((int) (DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP * DIP_SCALE))
                .when(mDisplay)
                .getDisplayHeight();
        mBottomWindowPadding = 45;

        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        newConfig.screenHeightDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
        int sideSpacing = mResourceProvider.getDropdownSideSpacing();
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
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_min_space_above_window_bottom);
        return (int) (getConfiguration().screenHeightDp * DIP_SCALE - top)
                - minHeightAboveWindowBottom;
    }

    private Configuration getConfiguration() {
        return mContext.getResources().getConfiguration();
    }

    @Test
    public void testOnToEdgeChange() {
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
    public void testRecalculateOmniboxAlignment_phone_popover() {
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        setBounds(mAnchorView, 10, ANCHOR_TOP, ANCHOR_WIDTH, ANCHOR_HEIGHT);
        mImpl.recalculateOmniboxAlignment();
        assertEquals(
                new OmniboxAlignment(
                        10, ANCHOR_TOP, ANCHOR_WIDTH, getExpectedHeight(ANCHOR_TOP), 0, 0, 0, 0),
                mImpl.getCurrentAlignment());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void
            testRecalculateOmniboxAlignment_narrowWindow_popover_fullWidthExpansionDisallowed() {
        mIsFullWidthExpansionAllowed = false;
        mFuseboxLayoutModeSupplier.set(FuseboxLayoutMode.SUGGESTIONS_POPOVER);
        setBounds(mAnchorView, 10, ANCHOR_TOP, ANCHOR_WIDTH, ANCHOR_HEIGHT);
        Configuration newConfig = getConfiguration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1;
        mImpl.onConfigurationChanged(newConfig);
        assertFalse(mImpl.isWideWindow());

        mImpl.recalculateOmniboxAlignment();

        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(ALIGNMENT_LEFT, alignment.left);
        assertEquals(ALIGNMENT_WIDTH, alignment.width);
    }

    @Test
    public void testDestroy_removesTopInsetObserver() {
        mImpl.destroy();
        verify(mTopInsetProvider).removeObserver(any(TopInsetProvider.Observer.class));
    }
}
