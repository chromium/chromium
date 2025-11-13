// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Unit tests for {@link AdjustedTopUiThemeColorProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdjustedTopUiThemeColorProviderUnitTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private TintObserver mTintObserver;
    @Mock private NativePage mNativePage;

    private static final @ColorInt int TAB_COLOR = Color.RED;

    private final ObservableSupplier<@Nullable Tab> mTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mActivityThemeColorSupplier =
            new ObservableSupplierImpl<>();

    private Context mContext;
    private AdjustedTopUiThemeColorProvider mAdjustedTopUiThemeColorProvider;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mTab.isThemingAllowed()).thenReturn(true);
        mActivityThemeColorSupplier.set(TAB_COLOR);

        mAdjustedTopUiThemeColorProvider =
                new AdjustedTopUiThemeColorProvider(
                        mContext,
                        mTabSupplier,
                        mActivityThemeColorSupplier,
                        /* isTablet= */ false,
                        /* allowThemingInNightMode= */ true,
                        /* allowBrightThemeColors= */ true,
                        /* allowThemingOnTablets= */ true);

        mAdjustedTopUiThemeColorProvider.addTintObserver(mTintObserver);
    }

    @Test
    public void testUpdateColor() {
        @ColorInt int themeColor = mContext.getColor(R.color.default_icon_color_tint_list);
        ColorStateList adjustedTint =
                AppCompatResources.getColorStateList(
                        mContext, R.color.default_icon_color_white_tint_list);
        @BrandedColorScheme int adjustedBrandedColorScheme = BrandedColorScheme.DARK_BRANDED_THEME;
        assertNotEquals(themeColor, adjustedTint.getDefaultColor());

        // Verifies that the mAdjustedTintObserver is notified with adjusted tint color when
        // the useLightIconTint is true.
        updateColorAndVerifyOnTintChange(
                themeColor,
                /* useLightIconTint= */ true,
                adjustedTint.getDefaultColor(),
                adjustedBrandedColorScheme);

        // Verifies that the mAdjustedTintObserver is notified with default tint color when
        // the useLightIconTint is false.
        updateColorAndVerifyOnTintChange(
                themeColor,
                /* useLightIconTint= */ false,
                themeColor,
                BrandedColorScheme.APP_DEFAULT);
    }

    private void updateColorAndVerifyOnTintChange(
            @ColorInt int themeColor,
            boolean useLightIconTint,
            @ColorInt int expectedTintColor,
            @BrandedColorScheme int expectedBrandedColorScheme) {
        ArgumentCaptor<ColorStateList> tintCaptor = ArgumentCaptor.forClass(ColorStateList.class);
        ArgumentCaptor<ColorStateList> activityTintCaptor =
                ArgumentCaptor.forClass(ColorStateList.class);
        clearInvocations(mTintObserver);
        when(mNativePage.useLightIconTint()).thenReturn(useLightIconTint);

        mAdjustedTopUiThemeColorProvider.updateColor(mTab, themeColor, /* shouldAnimate= */ false);
        verify(mTintObserver)
                .onTintChanged(
                        tintCaptor.capture(),
                        activityTintCaptor.capture(),
                        eq(expectedBrandedColorScheme));
        assertEquals(expectedTintColor, tintCaptor.getValue().getDefaultColor());
        assertEquals(expectedTintColor, activityTintCaptor.getValue().getDefaultColor());
    }
}
