// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarThemeColorProviderTest {
    private static final @ColorInt int TOP_COLOR = Color.RED;
    private static final @ColorInt int BOTTOM_COLOR = Color.BLUE;

    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private BottomUiThemeColorProvider mBottomUiThemeColorProvider;
    @Mock private Tab mTab;

    private Context mContext;
    private ToolbarThemeColorProvider mToolbarThemeColorProvider;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        ColorStateList defaultTintList =
                ContextCompat.getColorStateList(mContext, R.color.default_icon_color_tint_list);

        when(mTopUiThemeColorProvider.getThemeColor()).thenReturn(TOP_COLOR);
        when(mTopUiThemeColorProvider.getTint()).thenReturn(defaultTintList);
        when(mTopUiThemeColorProvider.getActivityFocusTint()).thenReturn(defaultTintList);
        when(mTopUiThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);

        when(mBottomUiThemeColorProvider.getThemeColor()).thenReturn(BOTTOM_COLOR);
        when(mBottomUiThemeColorProvider.getTint()).thenReturn(defaultTintList);
        when(mBottomUiThemeColorProvider.getActivityFocusTint()).thenReturn(defaultTintList);
        when(mBottomUiThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);

        when(mBrowserControlsStateProvider.getControlsPosition()).thenReturn(ControlsPosition.TOP);
    }

    @Test
    public void testDefault_ListensToTop() {
        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);
        assertEquals(TOP_COLOR, mToolbarThemeColorProvider.getThemeColor());
        verify(mTopUiThemeColorProvider).addThemeColorObserver(mToolbarThemeColorProvider);
        verify(mTopUiThemeColorProvider).addTintObserver(mToolbarThemeColorProvider);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testBottomBarEnabled_SwitchToBottom() {
        when(mBrowserControlsStateProvider.getControlsPosition())
                .thenReturn(ControlsPosition.BOTTOM);

        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);
        assertEquals(BOTTOM_COLOR, mToolbarThemeColorProvider.getThemeColor());
        verify(mBottomUiThemeColorProvider).addThemeColorObserver(mToolbarThemeColorProvider);
        verify(mBottomUiThemeColorProvider).addTintObserver(mToolbarThemeColorProvider);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testSwitchingAtRuntime() {
        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);
        assertEquals(TOP_COLOR, mToolbarThemeColorProvider.getThemeColor());

        mToolbarThemeColorProvider.onControlsPositionChanged(ControlsPosition.BOTTOM);
        assertEquals(BOTTOM_COLOR, mToolbarThemeColorProvider.getThemeColor());

        mToolbarThemeColorProvider.onControlsPositionChanged(ControlsPosition.TOP);
        assertEquals(TOP_COLOR, mToolbarThemeColorProvider.getThemeColor());
    }

    @Test
    public void testForwardEvents() {
        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);

        mToolbarThemeColorProvider.onThemeColorChanged(Color.GREEN, true);
        assertEquals(Color.GREEN, mToolbarThemeColorProvider.getThemeColor());

        ColorStateList newTint =
                ContextCompat.getColorStateList(
                        mContext, R.color.default_icon_color_light_tint_list);
        mToolbarThemeColorProvider.onTintChanged(newTint, newTint, BrandedColorScheme.INCOGNITO);
        assertEquals(newTint, mToolbarThemeColorProvider.getTint());
        assertEquals(newTint, mToolbarThemeColorProvider.getActivityFocusTint());
        assertEquals(
                BrandedColorScheme.INCOGNITO, mToolbarThemeColorProvider.getBrandedColorScheme());
    }

    @Test
    public void testGetThemeColorOrFallback() {
        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);

        @ColorInt int fallbackColor = Color.YELLOW;
        when(mTopUiThemeColorProvider.getThemeColorOrFallback(mTab, fallbackColor))
                .thenReturn(fallbackColor);

        assertEquals(
                fallbackColor,
                mToolbarThemeColorProvider.getThemeColorOrFallback(mTab, fallbackColor));
    }

    @Test
    public void testGetToolbarBackgroundColor() {
        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);

        @ColorInt int bgColor = Color.GREEN;
        when(mTopUiThemeColorProvider.getToolbarBackgroundColor(mTab)).thenReturn(bgColor);

        assertEquals(bgColor, mToolbarThemeColorProvider.getToolbarBackgroundColor(mTab));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testGetThemeColorOrFallback_WithBottomActive() {
        when(mBrowserControlsStateProvider.getControlsPosition())
                .thenReturn(ControlsPosition.BOTTOM);
        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);

        assertEquals(
                BOTTOM_COLOR,
                mToolbarThemeColorProvider.getThemeColorOrFallback(mTab, Color.YELLOW));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testGetToolbarBackgroundColor_WithBottomActive() {
        when(mBrowserControlsStateProvider.getControlsPosition())
                .thenReturn(ControlsPosition.BOTTOM);
        mToolbarThemeColorProvider =
                new ToolbarThemeColorProvider(
                        mContext,
                        mTopUiThemeColorProvider,
                        mBottomUiThemeColorProvider,
                        mBrowserControlsStateProvider);

        assertEquals(BOTTOM_COLOR, mToolbarThemeColorProvider.getToolbarBackgroundColor(mTab));
    }
}
