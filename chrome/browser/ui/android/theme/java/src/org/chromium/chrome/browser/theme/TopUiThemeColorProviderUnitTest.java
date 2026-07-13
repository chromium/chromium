// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** Unit tests for {@link TopUiThemeColorProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TopUiThemeColorProviderUnitTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private static final @ColorInt int ACTIVITY_COLOR = Color.GREEN;

    private Context mContext;
    private TopUiThemeColorProvider mProvider;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        when(mTab.getContext()).thenReturn(mContext);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.isThemingAllowed()).thenReturn(true);
        when(mTab.getThemeColor()).thenReturn(TabState.UNSPECIFIED_THEME_COLOR);

        mTabSupplier = ObservableSuppliers.createNullable();
        mProvider =
                new TopUiThemeColorProvider(
                        mContext,
                        mTabSupplier,
                        () -> ACTIVITY_COLOR,
                        /* isTablet= */ false,
                        /* allowThemingInNightMode= */ true,
                        /* allowBrightThemeColors= */ true,
                        /* allowThemingOnTablets= */ true);
        mTabSupplier.set(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    public void testOnSSLStateUpdated() {
        assertEquals(
                "Activity theme color should be used while theming is allowed",
                ACTIVITY_COLOR,
                mProvider.getThemeColor());

        when(mTab.isThemingAllowed()).thenReturn(false);
        mTabObserverCaptor.getValue().onSSLStateUpdated(mTab);

        assertEquals(
                "Default theme color should be used once theming is no longer allowed",
                ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false),
                mProvider.getThemeColor());
        assertEquals(
                "Default color scheme should be used once theming is no longer allowed",
                BrandedColorScheme.APP_DEFAULT,
                mProvider.getBrandedColorScheme());
    }
}
