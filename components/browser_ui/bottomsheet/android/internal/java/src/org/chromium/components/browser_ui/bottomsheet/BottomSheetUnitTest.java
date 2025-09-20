// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Unit tests for {@link BottomSheet}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetUnitTest {
    private static final int APP_HEADER_HEIGHT = 42;
    private static final int SHEET_CONTAINER_HEIGHT = 200;
    private static final int SHEET_PEEK_HEIGHT = 60;

    @Mock private ViewGroup mSheetContainer;
    @Mock private View mSheetBackground;
    @Mock private MarginLayoutParams mSheetLayoutParams;
    @Mock private BottomSheetContent mSheetContent;
    @Mock private TouchRestrictingFrameLayout mToolbarHolder;

    @Captor ArgumentCaptor<MarginLayoutParams> mMarginCaptor;

    private BottomSheet mBottomSheet;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mActivity = buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBottomSheet =
                (BottomSheet) LayoutInflater.from(mActivity).inflate(R.layout.bottom_sheet, null);
        when(mSheetContainer.getLayoutParams()).thenReturn(mSheetLayoutParams);
        when(mSheetContainer.getHeight()).thenReturn(SHEET_CONTAINER_HEIGHT);
        mBottomSheet.setSheetContainerForTesting(mSheetContainer);
        mBottomSheet.setToolbarHolderForTesting(mToolbarHolder);
        mBottomSheet.setSheetBackgroundForTesting(mSheetBackground);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchToDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
        verify(mSheetContainer).setLayoutParams(mMarginCaptor.capture());
        assertEquals(
                "Sheet container's top margin should be updated to account for app header height.",
                APP_HEADER_HEIGHT,
                mMarginCaptor.getValue().topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SwitchOutOfDesktopWindow() {
        // Sheet container in desktop window will use top margin = APP_HEADER_HEIGHT.
        mSheetLayoutParams.topMargin = APP_HEADER_HEIGHT;
        // Simulate switching out of desktop windowing mode.
        mBottomSheet.onAppHeaderHeightChanged(0);
        verify(mSheetContainer).setLayoutParams(mMarginCaptor.capture());
        assertEquals(
                "Sheet container's top margin should be reset.",
                0,
                mMarginCaptor.getValue().topMargin);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderHeightChanged_SameAsContainerTopMargin() {
        mSheetLayoutParams.topMargin = APP_HEADER_HEIGHT;
        mBottomSheet.onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
        // Avoid LayoutParams update when there's no change in the top margin.
        verify(mSheetContainer, never()).setLayoutParams(any());
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorAtFullHeightScrimmed_Dark() {
        doTestBackgroundColorFullHeightScrimmed(
                SemanticColorUtils.getColorSurfaceContainerHigh(mActivity));
    }

    @Test
    public void testBackgroundColorAtFullHeightScrimmed_Light() {
        doTestBackgroundColorFullHeightScrimmed(SemanticColorUtils.getSheetBgColor(mActivity));
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
                SemanticColorUtils.getSheetBgColor(mActivity),
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
                SemanticColorUtils.getSheetBgColor(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }

    @Test
    @Config(qualifiers = "night")
    public void testBackgroundColorTransitionInDark() {
        int offset = 130;
        int expectedColor =
                ColorUtils.overlayColor(
                        SemanticColorUtils.getSheetBgColor(mActivity),
                        SemanticColorUtils.getColorSurfaceContainerHigh(mActivity),
                        0.5f // fraction = (130 - 60) / (200 - 60) = 0.5
                        );
        showSheetThenScrollToHalfOffsetAndVerifyColor(offset, expectedColor);
    }

    @Test
    public void testBackgroundColorNoTransitionInLight() {
        // Sheet color does not transition in light mode.
        showSheetThenScrollToHalfOffsetAndVerifyColor(
                130, SemanticColorUtils.getSheetBgColor(mActivity));
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
    public void testBackgroundColorOverride_Transparent() {
        doReturn(true).when(mSheetContent).hasSolidBackgroundColor();
        doReturn(Color.TRANSPARENT).when(mSheetContent).getSheetBackgroundColorOverride();

        mBottomSheet.showContent(mSheetContent);

        assertEquals(
                "Sheet bg color should be the override color.",
                SemanticColorUtils.getSheetBgColor(mActivity),
                mBottomSheet.getSheetBackgroundColor());
    }
}
