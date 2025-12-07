// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

@RunWith(BaseRobolectricTestRunner.class)
public class BottomUiThemeColorProviderTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;
    @Mock private ThemeColorProvider mToolbarThemeColorProvider;
    private Context mContext;
    private BottomUiThemeColorProvider mColorProvider;
    private @ColorInt int mPrimaryBackgroundColorWithTopToolbar;
    private @ColorInt int mIncognitoBackgroundColorWithTopToolbar;
    private ColorStateList mPrimaryTintWithTopToolbar;
    private ColorStateList mIncognitoTintWithTopToolbar;
    private ColorStateList mToolbarTintList;
    private ColorStateList mToolbarTintOtherList;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mPrimaryBackgroundColorWithTopToolbar = SemanticColorUtils.getColorSurface(mContext);
        mIncognitoBackgroundColorWithTopToolbar = mContext.getColor(R.color.tab_strip_bg_incognito);
        mPrimaryTintWithTopToolbar =
                ContextCompat.getColorStateList(mContext, R.color.default_icon_color_tint_list);
        mIncognitoTintWithTopToolbar =
                ContextCompat.getColorStateList(
                        mContext, R.color.default_icon_color_light_tint_list);
        mToolbarTintList =
                ContextCompat.getColorStateList(
                        mContext, R.color.default_text_color_link_tint_list);
        mToolbarTintOtherList =
                ContextCompat.getColorStateList(
                        mContext, R.color.default_icon_color_white_tint_list);

        doReturn(Color.RED).when(mToolbarThemeColorProvider).getThemeColor();
        doReturn(mToolbarTintList).when(mToolbarThemeColorProvider).getTint();
        doReturn(mToolbarTintList).when(mToolbarThemeColorProvider).getActivityFocusTint();
        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();

        mColorProvider =
                new BottomUiThemeColorProvider(
                        mToolbarThemeColorProvider,
                        mBrowserControlsStateProvider,
                        mBottomControlsStacker,
                        mIncognitoStateProvider,
                        mContext);
        mColorProvider.onIncognitoStateChanged(false);
    }

    @Test
    public void testIncognitoStateChange() {
        assertEquals(mPrimaryBackgroundColorWithTopToolbar, mColorProvider.getThemeColor());
        assertEquals(mPrimaryTintWithTopToolbar, mColorProvider.getTint());

        mColorProvider.onIncognitoStateChanged(true);
        assertEquals(mIncognitoBackgroundColorWithTopToolbar, mColorProvider.getThemeColor());
        assertEquals(mIncognitoTintWithTopToolbar, mColorProvider.getTint());

        mColorProvider.onIncognitoStateChanged(false);
        assertEquals(mPrimaryBackgroundColorWithTopToolbar, mColorProvider.getThemeColor());
        assertEquals(mPrimaryTintWithTopToolbar, mColorProvider.getTint());
    }

    @Test
    public void testFollowToolbarColor() {
        mColorProvider.onControlsPositionChanged(ControlsPosition.BOTTOM);
        assertEquals(Color.RED, mColorProvider.getThemeColor());
        assertEquals(mToolbarTintList, mColorProvider.getTint());
        verify(mBottomControlsStacker).notifyBackgroundColor(Color.RED);

        mColorProvider.onIncognitoStateChanged(true);
        assertEquals(Color.RED, mColorProvider.getThemeColor());
        assertEquals(mToolbarTintList, mColorProvider.getTint());
        verify(mBottomControlsStacker, times(2)).notifyBackgroundColor(Color.RED);

        doReturn(mToolbarTintOtherList).when(mToolbarThemeColorProvider).getTint();
        mColorProvider.onTintChanged(
                mToolbarTintOtherList, mToolbarTintOtherList, BrandedColorScheme.APP_DEFAULT);
        assertEquals(mToolbarTintOtherList, mColorProvider.getTint());

        doReturn(Color.BLUE).when(mToolbarThemeColorProvider).getThemeColor();
        mColorProvider.onThemeColorChanged(Color.BLUE, false);
        assertEquals(Color.BLUE, mColorProvider.getThemeColor());
        verify(mBottomControlsStacker).notifyBackgroundColor(Color.BLUE);

        mColorProvider.onControlsPositionChanged(ControlsPosition.TOP);
        assertEquals(mIncognitoBackgroundColorWithTopToolbar, mColorProvider.getThemeColor());
        assertEquals(mIncognitoTintWithTopToolbar, mColorProvider.getTint());
        verify(mBottomControlsStacker, times(3)).notifyBackgroundColor(Color.RED);
    }
}
