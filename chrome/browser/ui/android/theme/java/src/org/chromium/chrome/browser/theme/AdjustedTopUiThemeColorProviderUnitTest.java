// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
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
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeStateProvider;
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
    @Mock private NtpThemeStateProvider mNtpThemeStateProvider;
    @Captor private ArgumentCaptor<NtpThemeStateProvider.Observer> mObserverCaptor;

    private static final @ColorInt int TAB_COLOR = Color.RED;

    private final SettableNonNullObservableSupplier<Integer> mActivityThemeColorSupplier =
            ObservableSuppliers.createNonNull(TAB_COLOR);

    private Context mContext;
    private AdjustedTopUiThemeColorProvider mAdjustedTopUiThemeColorProvider;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mTab.isThemingAllowed()).thenReturn(true);
        when(mTab.getThemeColor()).thenReturn(TAB_COLOR);

        mTabSupplier = ObservableSuppliers.createNullable(mTab);
        NtpThemeStateProvider.setInstanceForTesting(mNtpThemeStateProvider);

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
        verify(mNtpThemeStateProvider).addObserver(mObserverCaptor.capture());
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

    @Test
    public void testDestroy() {
        mAdjustedTopUiThemeColorProvider.destroy();
        verify(mNtpThemeStateProvider).removeObserver(eq(mObserverCaptor.getValue()));
    }

    @Test
    public void testOnCustomBackgroundChanged_nonNtp() {
        when(mNativePage.useLightIconTint()).thenReturn(true);
        when(mNativePage.supportsEdgeToEdge()).thenReturn(true);

        // 1. Tab is null.
        mTabSupplier.set(null);
        clearInvocations(mTintObserver);
        mObserverCaptor.getValue().onCustomBackgroundChanged();
        verify(mTintObserver, never()).onTintChanged(any(), any(), anyInt());

        // 2. Tab is not native page.
        mTabSupplier.set(mTab);
        when(mTab.isNativePage()).thenReturn(false);
        clearInvocations(mTintObserver);
        mObserverCaptor.getValue().onCustomBackgroundChanged();
        verify(mTintObserver, never()).onTintChanged(any(), any(), anyInt());

        // 3. Tab is native page but doesn't support edge-to-edge.
        when(mTab.isNativePage()).thenReturn(true);
        when(mNativePage.supportsEdgeToEdge()).thenReturn(false);
        clearInvocations(mTintObserver);
        mObserverCaptor.getValue().onCustomBackgroundChanged();
        verify(mTintObserver, never()).onTintChanged(any(), any(), anyInt());
    }

    @Test
    public void testOnCustomBackgroundChanged_Ntp() {
        ColorStateList adjustedTint =
                AppCompatResources.getColorStateList(
                        mContext, R.color.default_icon_color_white_tint_list);
        @BrandedColorScheme int adjustedBrandedColorScheme = BrandedColorScheme.DARK_BRANDED_THEME;
        when(mNativePage.supportsEdgeToEdge()).thenReturn(true);

        // Case NTP uses light tint color.
        when(mNativePage.useLightIconTint()).thenReturn(true);
        clearInvocations(mTintObserver);
        mObserverCaptor.getValue().onCustomBackgroundChanged();

        ArgumentCaptor<ColorStateList> tintCaptor = ArgumentCaptor.forClass(ColorStateList.class);
        ArgumentCaptor<ColorStateList> activityTintCaptor =
                ArgumentCaptor.forClass(ColorStateList.class);
        verify(mTintObserver)
                .onTintChanged(
                        tintCaptor.capture(),
                        activityTintCaptor.capture(),
                        eq(adjustedBrandedColorScheme));
        assertEquals(adjustedTint, tintCaptor.getValue());

        // Case NTP uses regular Tab's tint color.
        var themeColor =
                ThemeUtils.getThemedToolbarIconTint(mContext, BrandedColorScheme.APP_DEFAULT);
        when(mNativePage.useLightIconTint()).thenReturn(false);
        clearInvocations(mTintObserver);
        mObserverCaptor.getValue().onCustomBackgroundChanged();

        verify(mTintObserver)
                .onTintChanged(
                        tintCaptor.capture(),
                        activityTintCaptor.capture(),
                        eq(BrandedColorScheme.APP_DEFAULT));
        assertEquals(themeColor.getDefaultColor(), tintCaptor.getValue().getDefaultColor());
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
