// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;

/** Unit tests for {@link BottomBarConfigUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw300dp")
public class BottomBarConfigUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;

    @Mock private Tab mTab;
    @Mock private NativePage mNativePage;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Tablet() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Phone() {
        assertTrue(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Automotive() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarDisabled() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/false")
    public void testShouldIncludeHomeButtonIfEnabled_FalseParam() {
        assertTrue(BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/true")
    public void testShouldIncludeHomeButtonIfEnabled_TrueParam() {
        assertFalse(BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/false")
    public void testShouldIncludeAppMenuButton_FalseParam() {
        assertTrue(BottomBarConfigUtils.shouldIncludeAppMenuButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true")
    public void testShouldIncludeAppMenuButton_TrueParam() {
        assertFalse(BottomBarConfigUtils.shouldIncludeAppMenuButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_bottom_bar_on_gts/true")
    public void testShouldShowOnGts_TrueParam() {
        assertTrue(BottomBarConfigUtils.shouldShowOnGts());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_bottom_bar_on_gts/false")
    public void testShouldShowOnGts_FalseParam() {
        assertFalse(BottomBarConfigUtils.shouldShowOnGts());
    }

    @Test
    public void testIsNtpScrollOffEnabled_NullInputs() {
        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(null, mContext));
        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, null));
        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(null, null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false")
    public void testIsNtpScrollOffEnabled_ValidNtp() {
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertTrue(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_Incognito() {
        when(mTab.isIncognito()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_BottomBarDisabled() {
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR
                    + ":ntp_scroll_off_enabled/true/disable_on_ntp/false")
    public void testIsNtpScrollOffEnabled_KillSwitchEnabled() {
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertTrue(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":ntp_scroll_off_enabled/false")
    public void testIsNtpScrollOffEnabled_KillSwitchDisabled() {
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true")
    public void testIsNtpScrollOffEnabled_DisableOnNtpEnabled() {
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false")
    public void testIsNtpScrollOffEnabled_DisableOnNtpDisabled() {
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertTrue(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":always_use_filled_glic_icon/true")
    public void testAlwaysUseFilledIcon_TrueParam() {
        assertTrue(BottomBarConfigUtils.alwaysUseFilledIcon());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":always_use_filled_glic_icon/false")
    public void testAlwaysUseFilledIcon_FalseParam() {
        assertFalse(BottomBarConfigUtils.alwaysUseFilledIcon());
    }
}
