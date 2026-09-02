// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import static org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.MAX_HEIGHT_RATIO;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.ShadowLayerView;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.util.ColorUtils;

/** Unit tests for {@link BottomSheet}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetUnitTest {
    private static final int APP_HEADER_HEIGHT = 42;
    private static final int SHEET_CONTAINER_HEIGHT = 200;
    private static final int SHEET_CONTAINER_WIDTH = 1080;
    private static final int SHEET_PEEK_HEIGHT = 60;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mSheetBackground;
    @Mock private ShadowLayerView mShadowLayerView;
    @Mock private BottomSheetContent mSheetContent;
    @Mock private TouchRestrictingFrameLayout mToolbarHolder;
    @Mock private InsetObserver mInsetObserver;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock private BottomSheetObserver mBottomSheetObserver;

    @Captor
    private ArgumentCaptor<InsetObserver.WindowInsetsAnimationListener>
            mInsetsAnimationListenerCaptor;

    private SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier;
    private BottomSheet mBottomSheet;
    private ViewGroup mSheetContainer;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBottomSheet =
                (BottomSheet) LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet, null);

        FrameLayout sheetContainerParent = new FrameLayout(mActivity);
        mSheetContainer = new FrameLayout(mActivity);
        mSheetContainer.setLayoutParams(
                new MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, SHEET_CONTAINER_HEIGHT));

        sheetContainerParent.addView(mSheetContainer);
        mSheetContainer.addView(mBottomSheet);

        // Measure and layout the container so it has a size.
        mSheetContainer.measure(
                View.MeasureSpec.makeMeasureSpec(SHEET_CONTAINER_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(SHEET_CONTAINER_HEIGHT, View.MeasureSpec.EXACTLY));
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        mBottomSheet.setSheetContainerForTesting(mSheetContainer);
        mBottomSheet.setToolbarHolderForTesting(mToolbarHolder);
        mBottomSheet.setBottomSheetContentContainerForTesting(
                mBottomSheet.findViewById(R.id.bottom_sheet_content));

        mKeyboardInsetSupplier = ObservableSuppliers.createNonNull(0);
        doReturn(mKeyboardInsetSupplier).when(mInsetObserver).getSupplierForKeyboardInset();

        mBottomSheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ false);

        mBottomSheet.setSheetBackgroundForTesting(mSheetBackground);
        ViewGroup.MarginLayoutParams params = new ViewGroup.MarginLayoutParams(0, 0);
        doReturn(params).when(mShadowLayerView).getLayoutParams();
        mBottomSheet.setShadowLayerForTesting(mShadowLayerView);
        when(mSheetContent.getMaxResizeContentHeightRatio()).thenReturn(MAX_HEIGHT_RATIO);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    private void setupBottomSheetForKeyboardTest() {
        BottomSheet.setSmallScreenForTesting(false);
        when(mSheetContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        when(mSheetContent.getMaxResizeContentHeightRatio()).thenReturn(MAX_HEIGHT_RATIO);
        when(mSheetContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mSheetContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.HALF, false);

        View decorView = mActivity.getWindow().getDecorView();
        decorView.layout(0, 0, 1080, 1920);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT - 1);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        verify(mInsetObserver)
                .addWindowInsetsAnimationListener(mInsetsAnimationListenerCaptor.capture());
    }

    private void setupBottomSheetStrings(int openStringId, int closeStringId) {
        if (openStringId != 0) {
            doReturn(openStringId).when(mSheetContent).getSheetFullHeightAccessibilityStringId();
            doReturn(openStringId).when(mSheetContent).getSheetHalfHeightAccessibilityStringId();
        }
        if (closeStringId != 0) {
            doReturn(closeStringId).when(mSheetContent).getSheetHiddenAccessibilityStringId();
            doReturn(closeStringId).when(mSheetContent).getSheetClosedAccessibilityStringId();
        }
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchToDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
        MarginLayoutParams params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        assertEquals(
                "Sheet container's top margin should be updated to account for app header height.",
                APP_HEADER_HEIGHT,
                params.topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchOutOfDesktopWindow() {
        // Sheet container in desktop window will use top margin = APP_HEADER_HEIGHT.
        MarginLayoutParams params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        params.topMargin = APP_HEADER_HEIGHT;
        mSheetContainer.setLayoutParams(params);

        // Simulate switching out of desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(0);

        params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        assertEquals("Sheet container's top margin should be reset.", 0, params.topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SameAsContainerTopMargin() {
        MarginLayoutParams params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        params.topMargin = APP_HEADER_HEIGHT;
        mSheetContainer.setLayoutParams(params);

        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);

        assertEquals(
                "Sheet container's top margin should be unchanged.",
                APP_HEADER_HEIGHT,
                ((MarginLayoutParams) mSheetContainer.getLayoutParams()).topMargin);
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorAtFullHeightScrimmed_Dark() {
        doTestBackgroundColorFullHeightScrimmed(
                SemanticColorUtils.getColorSurfaceContainerHigh(mActivity));
    }

    @Test
    public void testBackgroundColorAtFullHeightScrimmed_Light() {
        doTestBackgroundColorFullHeightScrimmed(SemanticColorUtils.getColorSurface(mActivity));
    }

    private void doTestBackgroundColorFullHeightScrimmed(int expectedColor) {
        doReturn(HeightMode.DISABLED).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();
        doReturn(false).when(mSheetContent).hasCustomScrimLifecycle();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "Scrimmed sheet bg color is wrong.",
                expectedColor,
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorAtFullHeightUnscrimmed_Dark() {
        showFullHeightUnscrimmedSheetAndVerifyBackgroundColor();
    }

    @Test
    public void testBackgroundColorAtFullHeightUnscrimmed_Light() {
        showFullHeightUnscrimmedSheetAndVerifyBackgroundColor();
    }

    private void showFullHeightUnscrimmedSheetAndVerifyBackgroundColor() {
        doReturn(HeightMode.DISABLED).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();
        doReturn(true).when(mSheetContent).hasCustomScrimLifecycle();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "Unscrimmed sheet bg color is wrong.",
                SemanticColorUtils.getColorSurface(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testBackgroundColorAtPeekHeight_Light() {
        showSheetAtPeekHeightAndVerifyBackgroundColor();
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorAtPeekHeight_Dark() {
        showSheetAtPeekHeightAndVerifyBackgroundColor();
    }

    private void showSheetAtPeekHeightAndVerifyBackgroundColor() {
        doReturn(SHEET_PEEK_HEIGHT).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "PEEK state sheet bg color is wrong.",
                SemanticColorUtils.getColorSurface(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorTransitionInDark() {
        int offset = 130;
        int expectedColor =
                ColorUtils.overlayColor(
                        SemanticColorUtils.getColorSurface(mActivity),
                        SemanticColorUtils.getColorSurfaceContainerHigh(mActivity),
                        0.5f // fraction = (130 - 60) / (200 - 60) = 0.5
                        );
        showSheetThenScrollToHalfOffsetAndVerifyColor(offset, expectedColor);
    }

    @Test
    public void testBackgroundColorNoTransitionInLight() {
        // Sheet color does not transition in light mode.
        showSheetThenScrollToHalfOffsetAndVerifyColor(
                130, SemanticColorUtils.getColorSurface(mActivity));
    }

    private void showSheetThenScrollToHalfOffsetAndVerifyColor(int offset, int expectedColor) {
        doReturn(SHEET_PEEK_HEIGHT).when(mSheetContent).getPeekHeight();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetOffset(offset, false);

        assertEquals(
                "Half-height state sheet bg is different.",
                expectedColor,
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testBackgroundColorOverride() {
        final int overrideColor = Color.CYAN;
        doReturn(true).when(mSheetContent).hasSolidBackgroundColor();
        doReturn(overrideColor).when(mSheetContent).getSheetBackgroundColorOverride();

        mBottomSheet.showContent(mSheetContent);
        assertEquals(
                "Sheet bg color should be the override color.",
                overrideColor,
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testGetFullRatio_ResizeContent() {
        BottomSheet.setSmallScreenForTesting(false);
        when(mSheetContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Full ratio for RESIZE_CONTENT should be MAX_HEIGHT_RATIO.",
                MAX_HEIGHT_RATIO,
                mBottomSheet.getFullRatio(),
                0.0f);
    }

    @Test
    public void testGetFullRatio_ResizeContent_CustomMaxRatioCap() {
        BottomSheet.setSmallScreenForTesting(false);
        when(mSheetContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        when(mSheetContent.getMaxResizeContentHeightRatio()).thenReturn(0.80f);
        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Full ratio for RESIZE_CONTENT with custom max ratio cap should be 0.80f.",
                0.80f,
                mBottomSheet.getFullRatio(),
                0.0f);
    }

    @Test
    public void testSetSheetOffsetFromBottom_ResizeContent_CustomMaxRatioCap() {
        BottomSheet.setSmallScreenForTesting(false);
        when(mSheetContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        when(mSheetContent.getMaxResizeContentHeightRatio()).thenReturn(0.80f);
        // Return 0.5 for half height to make the min height 100 (container height is 200).
        // Max full height is 0.80 * 200 = 160.
        when(mSheetContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mSheetContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        setupBottomSheetStrings(
                R.string.bottom_sheet_accessibility_description,
                R.string.bottom_sheet_accessibility_description);
        when(mSheetContent.getContentView()).thenReturn(new View(mActivity));
        mBottomSheet.showContent(mSheetContent);

        mBottomSheet.getVisibleViewportRectForTesting().set(0, 0, 1080, 1920);

        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);

        mBottomSheet.setSheetOffsetFromBottom(120.0f, BottomSheetController.StateChangeReason.NONE);
        // Container height should be clamp(120, 100, 160) = 120.
        assertEquals(120, contentContainer.getLayoutParams().height);

        mBottomSheet.setSheetOffsetFromBottom(50.0f, BottomSheetController.StateChangeReason.NONE);
        // Container height should be clamp(50, 100, 160) = 100.
        assertEquals(100, contentContainer.getLayoutParams().height);

        mBottomSheet.setSheetOffsetFromBottom(180.0f, BottomSheetController.StateChangeReason.NONE);
        // Container height should be clamp(180, 100, 160) = 160.
        assertEquals(160, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testSetSheetState_Full_ResizeContent_CustomMaxRatioCap() {
        BottomSheet.setSmallScreenForTesting(false);
        when(mSheetContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        when(mSheetContent.getMaxResizeContentHeightRatio()).thenReturn(0.80f);
        when(mSheetContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mSheetContent.getPeekHeight()).thenReturn(HeightMode.DEFAULT);
        setupBottomSheetStrings(
                R.string.bottom_sheet_accessibility_description,
                R.string.bottom_sheet_accessibility_description);
        when(mSheetContent.getContentView()).thenReturn(new View(mActivity));
        mBottomSheet.showContent(mSheetContent);

        mBottomSheet.getVisibleViewportRectForTesting().set(0, 0, 1080, 1920);

        mBottomSheet.setSheetState(SheetState.FULL, false);

        // Max sheet height is 200, so 0.80 * 200 = 160.
        assertEquals(160.0f, mBottomSheet.getCurrentOffsetPx(), 0.0f);
        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);
        assertEquals(160, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testGetFullRatio_ResizeContent_HalfStateDisabled() {
        when(mSheetContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        when(mSheetContent.getHalfHeightRatio()).thenReturn((float) HeightMode.DISABLED);
        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Full ratio for RESIZE_CONTENT with half state disabled should be 1.0f.",
                MAX_HEIGHT_RATIO,
                mBottomSheet.getFullRatio(),
                0.0f);
    }

    @Test
    public void testSetSheetOffsetFromBottom_ResizeContent() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();

        // Return 0.5 for half height to make the min height 100 (container height is 200)
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        setupBottomSheetStrings(
                R.string.bottom_sheet_accessibility_description,
                R.string.bottom_sheet_accessibility_description);
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        mBottomSheet.showContent(mSheetContent);

        mBottomSheet.getVisibleViewportRectForTesting().set(0, 0, 1080, 1920);

        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);

        mBottomSheet.setSheetOffsetFromBottom(150.0f, BottomSheetController.StateChangeReason.NONE);
        // Container height should be max(100, 150) = 150.
        assertEquals(150, contentContainer.getLayoutParams().height);

        mBottomSheet.setSheetOffsetFromBottom(50.0f, BottomSheetController.StateChangeReason.NONE);
        // Container height should be max(100, 50) = 100.
        assertEquals(100, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testOnSheetContentChanged_ResizeContentRestore() {
        BottomSheet.setSmallScreenForTesting(false);

        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        setupBottomSheetStrings(
                R.string.bottom_sheet_accessibility_description,
                R.string.bottom_sheet_accessibility_description);
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        mBottomSheet.showContent(mSheetContent);

        mBottomSheet.getVisibleViewportRectForTesting().set(0, 0, 1080, 1920);

        mBottomSheet.setSheetOffsetFromBottom(150.0f, BottomSheetController.StateChangeReason.NONE);
        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);
        assertEquals(150, contentContainer.getLayoutParams().height);

        // Hide content (which means content == null in onSheetContentChanged)
        mBottomSheet.showContent(null);
        // The container's layout params height should be restored to MATCH_PARENT
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testBackgroundColorOverride_Transparent() {
        doReturn(true).when(mSheetContent).hasSolidBackgroundColor();
        doReturn(Color.TRANSPARENT).when(mSheetContent).getSheetBackgroundColorOverride();

        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Sheet bg color should be the override color.",
                SemanticColorUtils.getColorSurface(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    public void testKeyboardCurtainColor_BackgroundColorOverride() {
        final int overrideColor = Color.CYAN;
        doReturn(true).when(mSheetContent).hasSolidBackgroundColor();
        doReturn(overrideColor).when(mSheetContent).getSheetBackgroundColorOverride();

        mBottomSheet.showContent(mSheetContent);

        View curtain = mBottomSheet.findViewById(R.id.keyboard_curtain);
        assertEquals(
                "Keyboard curtain bg color should be the override color.",
                ColorStateList.valueOf(overrideColor),
                curtain.getBackgroundTintList());
    }

    @Test
    public void testBackgroundGlowColor() {
        final int glowColor = Color.CYAN;
        BottomSheetContent.GlowSpec spec =
                new BottomSheetContent.GlowSpec(
                        glowColor, BottomSheetContent.GlowSpec.ShadowSize.LONG);
        doReturn(spec).when(mSheetContent).getSheetBackgroundGlowSpecOverride();

        mBottomSheet.showContent(mSheetContent);

        verify(mShadowLayerView).setBackgroundTintList(ColorStateList.valueOf(glowColor));
        int expectedSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length_large);
        verify(mShadowLayerView).setShadowLength(expectedSize);
    }

    @Test
    public void testBackgroundGlowColor_Transparent() {
        doReturn(null).when(mSheetContent).getSheetBackgroundGlowSpecOverride();

        mBottomSheet.showContent(mSheetContent);

        verify(mShadowLayerView).setBackgroundTintList(null);
        int defaultSize =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length);
        verify(mShadowLayerView).setShadowLength(defaultSize);
    }

    @Test
    public void testApplyLargeFormFactorBackgroundBounds() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        doReturn(true).when(mSheetContent).supportsLargeFormFactor();

        // Stub layout properties that would normally be inflated or measured by Android framework
        // natively.
        final int shadowPaddingTop = 10;
        final int shadowPaddingBottom = 20;
        final int shadowTop = 10;
        final int backgroundTop = 20;
        final int backgroundMeasuredHeight = 300;
        final float userDragTranslationY = 100f;

        doReturn(shadowPaddingTop).when(mShadowLayerView).getPaddingTop();
        doReturn(shadowPaddingBottom).when(mShadowLayerView).getPaddingBottom();
        doReturn(shadowTop).when(mShadowLayerView).getTop();
        doReturn(backgroundTop).when(mSheetBackground).getTop();
        doReturn(backgroundMeasuredHeight).when(mSheetBackground).getMeasuredHeight();

        sheet.showContent(mSheetContent);

        // At this point, the layout and visibility should be initialized.
        // We will call the private method indirectly by triggering a layout pass or updating
        // translation.
        sheet.setSheetOffsetFromBottom(
                userDragTranslationY, BottomSheetController.StateChangeReason.NONE);

        // Evaluate exactly what boundaries the method derived:
        // visibleHeight = min(currentOffsetPx, measuredBgHeight) = min(100, 300) = 100.
        // backgroundBottom = backgroundTop + visibleHeight = 20 + 100 = 120.
        // shadowBottom = shadowTop + visibleHeight + shadowPaddingTop + shadowPaddingBottom
        //              = 10 + 100 + 10 + 20 = 140.
        verify(mSheetBackground, atLeastOnce()).setBottom(120);
        verify(mShadowLayerView, atLeastOnce()).setBottom(140);
    }

    @Test
    public void testBackgroundGlowColor_LargeFormFactor() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        doReturn(true).when(mSheetContent).supportsLargeFormFactor();

        int expectedSize =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length);
        doReturn(expectedSize).when(mShadowLayerView).getPaddingLeft();
        doReturn(expectedSize).when(mShadowLayerView).getPaddingTop();
        doReturn(expectedSize).when(mShadowLayerView).getPaddingRight();
        doReturn(expectedSize).when(mShadowLayerView).getPaddingBottom();

        sheet.showContent(mSheetContent);

        verify(mShadowLayerView).setBackgroundResource(R.drawable.popup_bg_shadow_16dp);
        ArgumentCaptor<ViewGroup.LayoutParams> captor =
                ArgumentCaptor.forClass(ViewGroup.LayoutParams.class);
        verify(mShadowLayerView, atLeastOnce()).setLayoutParams(captor.capture());
        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) captor.getValue();
        assertEquals(-expectedSize, params.leftMargin);
        assertEquals(-expectedSize, params.topMargin);
        assertEquals(-expectedSize, params.rightMargin);
        assertEquals(-expectedSize, params.bottomMargin);
    }

    @Test
    public void testWindowInsetsAnimationListener() {
        verify(mInsetObserver)
                .addWindowInsetsAnimationListener(mInsetsAnimationListenerCaptor.capture());
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        mBottomSheet.showContent(mSheetContent);

        mActivity.getWindow().getDecorView().layout(0, 0, 1080, 200);

        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);

        WindowInsetsCompat insets = mock(WindowInsetsCompat.class);
        doReturn(insets).when(mInsetObserver).getLastRawWindowInsets();

        // Full viewport.
        doReturn(Insets.of(0, 0, 0, 0)).when(insets).getInsets(anyInt());

        mBottomSheet.setSheetOffsetFromBottom(150.0f, BottomSheetController.StateChangeReason.NONE);
        assertEquals(150, contentContainer.getLayoutParams().height);

        // Simulate keyboard showing up, shrinking viewport to 100.
        doReturn(Insets.of(0, 0, 0, 100)).when(insets).getInsets(anyInt());

        listener.onStart(null, null);
        assertEquals(100, contentContainer.getLayoutParams().height);

        // Viewport shrinks to 50.
        doReturn(Insets.of(0, 0, 0, 150)).when(insets).getInsets(anyInt());

        listener.onProgress(null, null);
        assertEquals(50, contentContainer.getLayoutParams().height);

        // Viewport shrinks to 20.
        doReturn(Insets.of(0, 0, 0, 180)).when(insets).getInsets(anyInt());

        listener.onEnd(null);
        assertEquals(20, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testRevertStateOnKeyboardHiding() {
        setupBottomSheetForKeyboardTest();
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        WindowInsetsAnimationCompat imeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 50);
        listener.onPrepare(imeAnimation);

        mKeyboardInsetSupplier.set(100);
        listener.onStart(imeAnimation, null);

        // Simulate layout change while keyboard is showing (Pass 1)
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT - 10);

        // Simulate keyboard hiding.
        mKeyboardInsetSupplier.set(0);
        listener.onEnd(imeAnimation);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        // Verify that state is restored to HALF.
        assertEquals(SheetState.HALF, mBottomSheet.getSheetState());
    }

    @Test
    public void testRevertStateOnKeyboardHiding_ContainerShrinksBeforeOnStart() {
        setupBottomSheetForKeyboardTest();
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        // Simulate IME window insets animation preparing before layout pass.
        WindowInsetsAnimationCompat imeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 50);
        listener.onPrepare(imeAnimation);

        // Keyboard inset changes and container shrinks before onStart, causing
        // isHalfStateEnabled() to be false (forcing FULL).
        mKeyboardInsetSupplier.set(150);
        BottomSheet.setSmallScreenForTesting(true);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, 50);
        assertEquals(SheetState.FULL, mBottomSheet.getSheetState());
        assertEquals(SheetState.HALF, mBottomSheet.getStateBeforeKeyboardShownForTesting());

        listener.onStart(imeAnimation, null);

        // Restore screen to not small.
        BottomSheet.setSmallScreenForTesting(false);

        // Simulate keyboard hiding and container expanding back.
        mKeyboardInsetSupplier.set(0);
        listener.onEnd(imeAnimation);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        // Verify state is restored back to HALF.
        assertEquals(SheetState.HALF, mBottomSheet.getSheetState());
        assertEquals(SheetState.NONE, mBottomSheet.getStateBeforeKeyboardShownForTesting());
    }

    @Test
    public void testPreKeyboardStateCachedOnPrepare() {
        setupBottomSheetForKeyboardTest();
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        // Simulate IME window insets animation preparing.
        WindowInsetsAnimationCompat imeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 50);
        listener.onPrepare(imeAnimation);

        assertEquals(SheetState.HALF, mBottomSheet.getStateBeforeKeyboardShownForTesting());
        assertTrue(mBottomSheet.hasKeyboardTokenForTesting());

        // Keyboard inset appears and container shrinks before onStart, forcing sheet to FULL.
        mKeyboardInsetSupplier.set(150);
        BottomSheet.setSmallScreenForTesting(true);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, 50);
        assertEquals(SheetState.FULL, mBottomSheet.getSheetState());

        // Restore screen to not small.
        BottomSheet.setSmallScreenForTesting(false);

        // Keyboard hides.
        listener.onStart(imeAnimation, null);
        mKeyboardInsetSupplier.set(0);
        listener.onEnd(imeAnimation);

        // Container expands back to normal height.
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        assertEquals(SheetState.HALF, mBottomSheet.getSheetState());
        assertEquals(SheetState.NONE, mBottomSheet.getStateBeforeKeyboardShownForTesting());
    }

    @Test
    public void testNonImeAnimationOnPrepare_DoesNotCacheState() {
        setupBottomSheetForKeyboardTest();
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        // Simulate non-IME animation (e.g. status bars).
        WindowInsetsAnimationCompat statusBarAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.statusBars(), null, 50);
        listener.onPrepare(statusBarAnimation);

        // Pre-keyboard state should NOT be cached for non-IME animations.
        assertEquals(SheetState.NONE, mBottomSheet.getStateBeforeKeyboardShownForTesting());
        assertFalse(mBottomSheet.hasKeyboardTokenForTesting());
    }

    @Test
    public void testCancelRevertStateDueToHeightChange() {
        setupBottomSheetForKeyboardTest();
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        // Simulate keyboard showing
        WindowInsetsAnimationCompat imeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 50);
        listener.onPrepare(imeAnimation);

        mKeyboardInsetSupplier.set(100);
        listener.onStart(imeAnimation, null);

        // Simulate layout change while keyboard is showing
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT - 10);

        // Change state while keyboard is showing
        mBottomSheet.setSheetState(SheetState.FULL, false);

        // Simulate screen height change
        View decorView = mActivity.getWindow().getDecorView();
        decorView.layout(0, 0, 1080, 1820);

        // Simulate keyboard hiding.
        mKeyboardInsetSupplier.set(0);
        listener.onEnd(imeAnimation);

        // Trigger layout change on container
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        // Verify that state is NOT restored to HALF, but stays FULL.
        assertEquals(SheetState.FULL, mBottomSheet.getSheetState());
    }

    @Test
    public void testRecreateStateOnKeyboardShowingWithHeightChange() {
        setupBottomSheetForKeyboardTest();
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        WindowInsetsAnimationCompat imeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 50);
        listener.onPrepare(imeAnimation);

        mKeyboardInsetSupplier.set(100);
        listener.onStart(imeAnimation, null);

        // Simulate layout change while keyboard is showing.
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT - 10);

        // Simulate screen height change.
        View decorView = mActivity.getWindow().getDecorView();
        decorView.layout(0, 0, 1080, 1820);
        mSheetContainer.layout(0, 0, SHEET_CONTAINER_WIDTH, SHEET_CONTAINER_HEIGHT);

        listener.onPrepare(imeAnimation);
        mKeyboardInsetSupplier.set(150);
        listener.onStart(imeAnimation, null);
    }

    @Test
    public void triggerObserverOnInsetChange() {
        mBottomSheet.addObserver(mBottomSheetObserver);
        verify(mInsetObserver)
                .addWindowInsetsAnimationListener(mInsetsAnimationListenerCaptor.capture());
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        WindowInsetsAnimationCompat animation = new WindowInsetsAnimationCompat(0, null, 50);
        listener.onPrepare(animation);
        verify(mBottomSheetObserver).beforeInsetAnimationStart();

        listener.onEnd(animation);
        verify(mBottomSheetObserver).onInsetAnimationEnd();
    }

    @Test
    public void testUpdateA11yPaneTitle() {
        int openStringId = android.R.string.ok;
        int closedStringId = android.R.string.cancel;
        setupBottomSheetStrings(openStringId, closedStringId);

        mBottomSheet.showContent(mSheetContent);

        // Verify title when opened (Full Height)
        mBottomSheet.setSheetState(SheetState.FULL, false);
        CharSequence expectedOpenTitle = mActivity.getResources().getString(openStringId);
        assertEquals(
                "Accessibility pane title should be set on the bottom sheet when opened",
                expectedOpenTitle,
                ViewCompat.getAccessibilityPaneTitle(mBottomSheet));

        // Verify title when closed
        mBottomSheet.setSheetState(SheetState.HIDDEN, false);
        CharSequence expectedClosedTitle = mActivity.getResources().getString(closedStringId);
        assertEquals(
                "Accessibility pane title should be set to closed on the bottom sheet when"
                        + " closed",
                expectedClosedTitle,
                ViewCompat.getAccessibilityPaneTitle(mBottomSheet));
    }

    @Test
    public void testSheetContentDisplayedAndUnmaskedWhenOpened() {
        int openStringId = android.R.string.ok;
        int closedStringId = android.R.string.cancel;
        setupBottomSheetStrings(openStringId, closedStringId);
        // Explicitly mock legacy getSheetContentDescription as a negative test verification to
        // ensure
        // BottomSheet ignores it and maintains null contentDescription so inner child views are
        // unmasked.
        doReturn("Test Sheet Content Description")
                .when(mSheetContent)
                .getSheetContentDescription(any());
        doReturn(true).when(mSheetContent).swipeToDismissEnabled();

        TextView childView = new TextView(mActivity);
        childView.setText("Non-interactive title text");
        doReturn(childView).when(mSheetContent).getContentView();

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.FULL, false);

        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);
        assertEquals(
                "Child content view should be displayed inside the bottom sheet content container.",
                contentContainer,
                childView.getParent());
        assertEquals(
                "Child content view should be visible on screen when opened.",
                View.VISIBLE,
                childView.getVisibility());
        assertNull(
                "Root BottomSheet should have null contentDescription so child text is unmasked.",
                mBottomSheet.getContentDescription());
        assertNull(
                "Inner content container should have null contentDescription.",
                contentContainer.getContentDescription());
        assertEquals(
                "Accessibility pane title should still be set when opened.",
                mActivity.getResources().getString(openStringId),
                ViewCompat.getAccessibilityPaneTitle(mBottomSheet));
    }

    @Test
    public void testKeyboardStateResetOnContentChange() {
        BottomSheet.setSmallScreenForTesting(false);
        when(mSheetContent.getContentView()).thenReturn(new View(mActivity));

        // Configure content to be resizable.
        when(mSheetContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(mSheetContent.getFullHeightRatio()).thenReturn((float) HeightMode.RESIZE_CONTENT);
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);

        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.HALF, false);

        verify(mInsetObserver)
                .addWindowInsetsAnimationListener(mInsetsAnimationListenerCaptor.capture());
        InsetObserver.WindowInsetsAnimationListener listener =
                mInsetsAnimationListenerCaptor.getValue();

        // Simulate keyboard showing -> token acquired.
        WindowInsetsAnimationCompat imeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 50);
        listener.onPrepare(imeAnimation);
        mKeyboardInsetSupplier.set(100);
        listener.onStart(imeAnimation, null);

        assertTrue("Keyboard token should be acquired.", mBottomSheet.hasKeyboardTokenForTesting());
        assertEquals(
                "State before keyboard shown should be cached.",
                SheetState.HALF,
                mBottomSheet.getStateBeforeKeyboardShownForTesting());

        // Show different content (mock another content).
        BottomSheetContent newContent = mock();
        when(newContent.getContentView()).thenReturn(new View(mActivity));
        when(newContent.getHalfHeightRatio()).thenReturn(0.5f);
        when(newContent.getFullHeightRatio()).thenReturn((float) HeightMode.DEFAULT);
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);

        mBottomSheet.showContent(newContent);

        assertFalse(
                "Keyboard token should be reset when content changes.",
                mBottomSheet.hasKeyboardTokenForTesting());
        assertEquals(
                "State before keyboard shown should be reset.",
                SheetState.NONE,
                mBottomSheet.getStateBeforeKeyboardShownForTesting());
    }

    @Test
    public void testContentContainerHeightUpdated_ConstantTranslationY() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        doReturn((float) HeightMode.RESIZE_CONTENT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DISABLED).when(mSheetContent).getPeekHeight();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetHalfHeightAccessibilityStringId();
        doReturn(android.R.string.ok).when(mSheetContent).getSheetFullHeightAccessibilityStringId();

        mBottomSheet.showContent(mSheetContent);

        // Lay out decor view to have a large viewport.
        View decorView = mActivity.getWindow().getDecorView();
        decorView.layout(0, 0, 1080, 1000);

        // Set state to HALF. Offset should be HALF height (0.5 * 200 = 100).
        mBottomSheet.setSheetState(SheetState.HALF, false);

        View contentContainer = mBottomSheet.findViewById(R.id.bottom_sheet_content);

        // Now set offset to 200 (full height). translationY should be 0.
        mBottomSheet.setSheetOffsetFromBottom(200, StateChangeReason.NONE);
        assertEquals(200, contentContainer.getLayoutParams().height);
        assertEquals(0f, mBottomSheet.getTranslationY(), 0.0f);

        // Now set offset to 250. translationY should still be 0 (capped).
        // Height should remain clamped to FULL height (200).
        mBottomSheet.setSheetOffsetFromBottom(250, StateChangeReason.NONE);

        // Height should remain clamped to FULL height (200).
        assertEquals(200, contentContainer.getLayoutParams().height);
        assertEquals(0f, mBottomSheet.getTranslationY(), 0.0f);
    }

    @Test
    public void testDesktopUi_LargeFormFactorSupported() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(true).when(mSheetContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);
        doReturn((float) HeightMode.DEFAULT).when(mSheetContent).getFullHeightRatio();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();

        sheet.showContent(mSheetContent);

        assertEquals(
                "Max width should be desktop width.",
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_width),
                sheet.getMaxSheetWidth());

        assertEquals(
                "Bottom margin should be updated.",
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin),
                sheet.getContainerBottomMargin());
    }

    @Test
    public void testLargeFormFactorUi_DimensionsClamped() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);

        // Use a container smaller than the LFF sheet width to force clamping
        int narrowContainerWidth = 300;
        int shortContainerHeight = 300;
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);

        // Remeasure the container to a small size
        mSheetContainer.measure(
                View.MeasureSpec.makeMeasureSpec(narrowContainerWidth, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(shortContainerHeight, View.MeasureSpec.EXACTLY));
        mSheetContainer.layout(0, 0, narrowContainerWidth, shortContainerHeight);

        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(true).when(mSheetContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);
        sheet.showContent(mSheetContent);

        int edgeGap =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_edge_gap);
        int topGap =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin);

        assertEquals(
                "Max width should be clamped to ensure horizontal gaps for LFF.",
                Math.max(0, narrowContainerWidth - 2 * edgeGap),
                sheet.getMaxSheetWidth());

        sheet.setSheetState(SheetState.FULL, false);

        assertEquals(
                "Max height should be clamped to ensure a top gap for LFF.",
                Math.max(0, shortContainerHeight - sheet.getContainerBottomMargin() - topGap),
                (int) sheet.getCurrentOffsetPx());
    }

    @Test
    public void testLargeFormFactorUi_CloseButtonVisibility_NonModal() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(true).when(mSheetContent).hasCustomScrimLifecycle();
        doReturn(true).when(mSheetContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);

        sheet.showContent(mSheetContent);

        View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
        assertEquals(
                "Close button should be visible for non-modal sheets on large form factors.",
                View.VISIBLE,
                closeButton.getVisibility());
    }

    @Test
    public void testLargeFormFactorUi_CloseButtonVisibility_Modal() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(false).when(mSheetContent).hasCustomScrimLifecycle();
        doReturn(true).when(mSheetContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);

        sheet.showContent(mSheetContent);

        View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
        assertEquals(
                "Close button should be hidden for modal sheets on large form factors.",
                View.GONE,
                closeButton.getVisibility());
    }

    @Test
    public void testLargeFormFactorUi_CloseButtonVisibility_TransitionsFromNonModalToModal() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        BottomSheetContent nonModalContent = mock(BottomSheetContent.class);
        doReturn(true).when(nonModalContent).hasCustomScrimLifecycle();
        doReturn(true).when(nonModalContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(nonModalContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);

        sheet.showContent(nonModalContent);
        View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
        assertEquals(
                "Close button should be visible for non-modal sheets on large form factors.",
                View.VISIBLE,
                closeButton.getVisibility());

        BottomSheetContent modalContent = mock(BottomSheetContent.class);
        doReturn(false).when(modalContent).hasCustomScrimLifecycle();
        doReturn(true).when(modalContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(modalContent).getContentView();

        sheet.showContent(modalContent);
        assertEquals(
                "Close button should be hidden when transitioning to modal content on desktop.",
                View.GONE,
                closeButton.getVisibility());
    }

    @Test
    public void testSmallFormFactorUi_CloseButtonAlwaysHidden() {
        BottomSheet sheet =
                (BottomSheet) LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ false);

        doReturn(true).when(mSheetContent).hasCustomScrimLifecycle();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);

        sheet.showContent(mSheetContent);

        View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
        assertNull("Close button should never show on phones.", closeButton);
    }

    @Test
    public void testDesktopUi_LargeFormFactorNotSupported_FallbackToMobileRendering() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));

        // Use the real views found inside bottom_sheet_desktop, rather than mocks, to verify exact
        // layout behaviors.
        View sheetBackground = sheet.findViewById(R.id.background);
        View shadowLayer = sheet.findViewById(R.id.shadow_layer);
        View fallbackShadowLayer = sheet.findViewById(R.id.desktop_fallback_shadow);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(false).when(mSheetContent).supportsLargeFormFactor();
        doReturn(true).when(mSheetContent).hasCustomScrimLifecycle();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);
        doReturn((float) HeightMode.DEFAULT).when(mSheetContent).getFullHeightRatio();
        doReturn((float) HeightMode.DISABLED).when(mSheetContent).getHalfHeightRatio();
        doReturn(HeightMode.DEFAULT).when(mSheetContent).getPeekHeight();

        sheet.showContent(mSheetContent);

        assertEquals(
                "Fallback shadow layer should be visible.",
                View.VISIBLE,
                fallbackShadowLayer.getVisibility());

        assertEquals(
                "Close button should be hidden unconditionally during fallback.",
                View.GONE,
                sheet.findViewById(R.id.bottom_sheet_close_button).getVisibility());

        assertFalse(
                "Background clipToOutline should be disabled to prevent clipping the layout.",
                sheetBackground.getClipToOutline());

        assertEquals(
                "Wrapper shadow layer padding should be 0 to prevent pinching.",
                0,
                shadowLayer.getPaddingLeft());

        // Max width should not be large form factor width
        assertFalse(
                "Max width should not be desktop width.",
                mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_width)
                        == sheet.getMaxSheetWidth());
    }

    @Test
    public void testTargetState_ExpandingFromPeek() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(SHEET_PEEK_HEIGHT).when(mSheetContent).getPeekHeight();
        setupBottomSheetStrings(
                R.string.bottom_sheet_accessibility_description,
                R.string.bottom_sheet_accessibility_description);
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        mBottomSheet.showContent(mSheetContent);
        mBottomSheet.setSheetState(SheetState.PEEK, false);

        int targetState =
                mBottomSheet.forceScrollingStateForTesting(
                        SHEET_PEEK_HEIGHT + 20, /* yUpwardsVelocity= */ 1.0f);
        assertEquals(SheetState.HALF, targetState);
    }

    @Test
    public void testToggleSheetState() {
        BottomSheet.setSmallScreenForTesting(false);
        doReturn((float) HeightMode.DEFAULT).when(mSheetContent).getFullHeightRatio();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(SHEET_PEEK_HEIGHT).when(mSheetContent).getPeekHeight();

        setupBottomSheetStrings(
                R.string.bottom_sheet_accessibility_description,
                R.string.bottom_sheet_accessibility_description);
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();

        mBottomSheet.showContent(mSheetContent);

        mBottomSheet.setSheetState(SheetState.PEEK, false);
        assertEquals(SheetState.PEEK, mBottomSheet.getSheetState());

        mBottomSheet.toggleSheetState();
        assertEquals(SheetState.HALF, mBottomSheet.getTargetSheetState());
        mBottomSheet.endAnimations();
        assertEquals(SheetState.HALF, mBottomSheet.getSheetState());

        mBottomSheet.toggleSheetState();
        assertEquals(SheetState.FULL, mBottomSheet.getTargetSheetState());
        mBottomSheet.endAnimations();
        assertEquals(SheetState.FULL, mBottomSheet.getSheetState());

        mBottomSheet.toggleSheetState();
        assertEquals(SheetState.HALF, mBottomSheet.getTargetSheetState());
        mBottomSheet.endAnimations();
        assertEquals(SheetState.HALF, mBottomSheet.getSheetState());
    }

    @Test
    public void testDesktopHandlebarConfigurationFromContent() {
        // Initialize a Large Form Factor BottomSheet via layout XML that contains the desktop
        // layout
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);

        // Inject the newly created Sheet into the testing container
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setShadowLayerForTesting(mShadowLayerView);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true); // Force LFF enabled

        ImageView handlebar = sheet.getHandlebarForTesting();
        assertNotNull(handlebar);
        assertTrue(
                "Handlebar should have an OnClickListener configured on desktop",
                handlebar.hasOnClickListeners());
        int expectedPadding =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.bottom_sheet_handlebar_padding_vertical_desktop);
        assertEquals(
                "Handlebar top padding should be 8dp on desktop",
                expectedPadding,
                handlebar.getPaddingTop());
        assertEquals(
                "Handlebar bottom padding should be 8dp on desktop",
                expectedPadding,
                handlebar.getPaddingBottom());

        // Setup Sheet Content that requests a handlebar
        BottomSheetContent contentWithHandlebar = mock(BottomSheetContent.class);
        doReturn(true).when(contentWithHandlebar).supportsLargeFormFactor();
        doReturn(true).when(contentWithHandlebar).showHandlebar();
        doReturn(new View(mActivity)).when(contentWithHandlebar).getContentView();

        sheet.showContent(contentWithHandlebar);
        assertEquals(View.VISIBLE, handlebar.getVisibility());
        assertNotNull(
                "Handlebar should have TYPE_HAND hover pointer icon configured on desktop",
                handlebar.getPointerIcon());
        TouchRestrictingFrameLayout contentContainer =
                sheet.findViewById(R.id.bottom_sheet_content);
        MarginLayoutParams params = (MarginLayoutParams) contentContainer.getLayoutParams();
        assertEquals(handlebar.getMeasuredHeight(), params.topMargin);

        // Setup Sheet Content that does not request a handlebar
        BottomSheetContent contentWithoutHandlebar = mock(BottomSheetContent.class);
        doReturn(true).when(contentWithoutHandlebar).supportsLargeFormFactor();
        doReturn(false).when(contentWithoutHandlebar).showHandlebar();
        doReturn(new View(mActivity)).when(contentWithoutHandlebar).getContentView();

        sheet.showContent(contentWithoutHandlebar);
        assertEquals(View.GONE, handlebar.getVisibility());
        params = (MarginLayoutParams) contentContainer.getLayoutParams();
        assertEquals(0, params.topMargin);
    }

    @Test
    public void testWrapContentHeightIncludesHandlebarHeight() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        mSheetContainer.layout(0, 0, 1000, 800);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setShadowLayerForTesting(mShadowLayerView);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        View contentView =
                new View(mActivity) {
                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        setMeasuredDimension(
                                getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec), 200);
                    }
                };

        BottomSheetContent contentWithHandlebar = mock(BottomSheetContent.class);
        doReturn(true).when(contentWithHandlebar).supportsLargeFormFactor();
        doReturn(true).when(contentWithHandlebar).showHandlebar();
        doReturn((float) HeightMode.WRAP_CONTENT).when(contentWithHandlebar).getFullHeightRatio();
        doReturn(contentView).when(contentWithHandlebar).getContentView();

        sheet.showContent(contentWithHandlebar);
        ImageView handlebar = sheet.getHandlebarForTesting();
        int expectedHeight = 200 + handlebar.getMeasuredHeight();
        assertEquals(
                expectedHeight, sheet.getSheetHeightForState(SheetState.FULL), MathUtils.EPSILON);
        TouchRestrictingFrameLayout contentContainer =
                sheet.findViewById(R.id.bottom_sheet_content);
        MarginLayoutParams params = (MarginLayoutParams) contentContainer.getLayoutParams();
        assertEquals(handlebar.getMeasuredHeight(), params.topMargin);
    }

    @Test
    public void testPeekHeightIncludesHandlebarHeight() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setShadowLayerForTesting(mShadowLayerView);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        BottomSheetContent contentWithHandlebar = mock(BottomSheetContent.class);
        doReturn(true).when(contentWithHandlebar).supportsLargeFormFactor();
        doReturn(true).when(contentWithHandlebar).showHandlebar();
        doReturn(50).when(contentWithHandlebar).getPeekHeight();
        doReturn(new View(mActivity)).when(contentWithHandlebar).getContentView();

        sheet.showContent(contentWithHandlebar);
        ImageView handlebar = sheet.getHandlebarForTesting();
        int expectedPeekHeight = 50 + handlebar.getMeasuredHeight();
        assertEquals(expectedPeekHeight, sheet.getPeekHeightPx());
    }

    @Test
    public void testGetMaxSheetHeight_Standard() {
        assertEquals(mSheetContainer.getHeight(), mBottomSheet.getMaxSheetHeight());
    }

    @Test
    public void testGetMaxSheetHeight_LargeFormFactor() {
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        int containerHeight = 800;
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        mSheetContainer.layout(0, 0, 1000, containerHeight);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(true).when(mSheetContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);
        sheet.showContent(mSheetContent);

        int bottomMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin);
        int expectedMaxHeight = containerHeight - 2 * bottomMargin;
        assertEquals(expectedMaxHeight, sheet.getMaxSheetHeight());
    }

    @Test
    public void testAllowShadowOverflow_DisablesContainerClipping() {
        BottomSheet.setSmallScreenForTesting(false);
        int containerHeight = 1000;
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        mSheetContainer.layout(0, 0, 1000, containerHeight);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        sheet.setBottomSheetContentContainerForTesting(
                sheet.findViewById(R.id.bottom_sheet_content));
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(true).when(mSheetContent).supportsLargeFormFactor();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);
        sheet.showContent(mSheetContent);

        // Trigger onLayout in LFF mode.
        sheet.layout(0, 0, 1000, containerHeight);

        assertFalse(mSheetContainer.getClipChildren());
        assertFalse(mSheetContainer.getClipToPadding());
    }

    @Test
    public void testContentContainerHeight_LargeFormFactor() {
        BottomSheet.setSmallScreenForTesting(false);
        int containerHeight = 1000;
        BottomSheet sheet =
                (BottomSheet)
                        LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet_desktop, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        mSheetContainer.layout(0, 0, 1000, containerHeight);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        TouchRestrictingFrameLayout contentContainer =
                sheet.findViewById(R.id.bottom_sheet_content);
        sheet.setBottomSheetContentContainerForTesting(contentContainer);
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ true);

        doReturn(true).when(mSheetContent).supportsLargeFormFactor();
        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(1.0f).when(mSheetContent).getFullHeightRatio();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);
        sheet.showContent(mSheetContent);

        // When resting in half state, container height must be bounded to half height.
        sheet.setSheetState(SheetState.HALF, false);
        assertEquals(
                (int) (sheet.getMaxSheetHeight() * 0.5f),
                contentContainer.getLayoutParams().height);

        // When resting in full state, container height must expand to full height.
        sheet.setSheetState(SheetState.FULL, false);
        assertEquals(sheet.getMaxSheetHeight(), contentContainer.getLayoutParams().height);

        // For wrap-content sheets, container height is WRAP_CONTENT.
        BottomSheetContent wrapContent = mock(BottomSheetContent.class);
        doReturn(true).when(wrapContent).supportsLargeFormFactor();
        doReturn((float) HeightMode.WRAP_CONTENT).when(wrapContent).getFullHeightRatio();
        doReturn(new View(mActivity)).when(wrapContent).getContentView();
        doReturn(android.R.string.ok).when(wrapContent).getSheetFullHeightAccessibilityStringId();
        doReturn(android.R.string.ok).when(wrapContent).getSheetHalfHeightAccessibilityStringId();
        doReturn(android.R.string.ok).when(wrapContent).getSheetHiddenAccessibilityStringId();
        doReturn(android.R.string.ok).when(wrapContent).getSheetClosedAccessibilityStringId();
        sheet.showContent(wrapContent);
        assertEquals(
                ViewGroup.LayoutParams.WRAP_CONTENT, contentContainer.getLayoutParams().height);
    }

    @Test
    public void testContentContainerHeight_StandardFormFactor() {
        BottomSheet.setSmallScreenForTesting(false);
        int containerHeight = 1000;
        BottomSheet sheet =
                (BottomSheet) LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet, null);
        mSheetContainer.removeAllViews();
        mSheetContainer.addView(sheet);
        mSheetContainer.layout(0, 0, 1000, containerHeight);
        sheet.setSheetContainerForTesting(mSheetContainer);
        sheet.setToolbarHolderForTesting(mToolbarHolder);
        TouchRestrictingFrameLayout contentContainer =
                sheet.findViewById(R.id.bottom_sheet_content);
        sheet.setBottomSheetContentContainerForTesting(contentContainer);
        sheet.setSheetBackgroundForTesting(mSheetBackground);
        sheet.setShadowLayerForTesting(mShadowLayerView);

        sheet.init(
                mActivity.getWindow(),
                /* keyboardDelegate= */ mKeyboardDelegate,
                /* alwaysFullWidth= */ false,
                /* edgeToEdgeBottomInsetSupplier= */ () -> 0,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                /* isLargeFormFactor= */ false);

        doReturn(0.5f).when(mSheetContent).getHalfHeightRatio();
        doReturn(1.0f).when(mSheetContent).getFullHeightRatio();
        doReturn(new View(mActivity)).when(mSheetContent).getContentView();
        setupBottomSheetStrings(android.R.string.ok, android.R.string.ok);
        sheet.showContent(mSheetContent);

        // Standard form factor non-wrap sheets use MATCH_PARENT.
        sheet.setSheetState(SheetState.HALF, false);
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT, contentContainer.getLayoutParams().height);

        sheet.setSheetState(SheetState.FULL, false);
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT, contentContainer.getLayoutParams().height);
    }
}
