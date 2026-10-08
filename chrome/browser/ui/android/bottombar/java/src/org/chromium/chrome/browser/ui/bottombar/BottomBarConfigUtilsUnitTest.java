// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
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
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link BottomBarConfigUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw300dp")
public class BottomBarConfigUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;

    @Mock private Tab mTab;
    @Mock private NativePage mNativePage;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.BOTTOM_BAR_ENABLED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.BOTTOM_BAR_GLIC_BUTTON_ENABLED);
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
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_UserDisabled() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_UserEnabledFlagDisabled() {
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/true")
    public void testIsBottomBarUserEnabled_PrefUnset_DefaultParamTrue() {
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/false")
    public void testIsBottomBarUserEnabled_PrefUnset_DefaultParamFalse() {
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/false")
    public void testIsBottomBarUserEnabled_UserChoiceWinsOverDefaultParamFalse() {
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/true")
    public void testIsBottomBarUserEnabled_UserChoiceWinsOverDefaultParamTrue() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":default_user_enabled/false")
    public void testIsBottomBarEnabled_DefaultParamFalse_PrefUnset() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    public void testSetBottomBarUserEnabled_RoundTrip() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
        BottomBarConfigUtils.setBottomBarUserEnabled(true);
        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_Eligible_ParamTrue() {
        assertTrue(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/false")
    public void testShouldShowSettingsToggle_Eligible_ParamFalse() {
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowSettingsToggle_FlagDisabled_ParamDefaultsTrue() {
        // show_settings_toggle defaults to true; the flag being off must still hide the toggle.
        assertTrue(ChromeFeatureList.sAndroidBottomBarShowSettingsToggle.getValue());
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_Tablet_ParamTrue() {
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_Automotive_ParamTrue() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_settings_toggle/true")
    public void testShouldShowSettingsToggle_IgnoresUserPref() {
        BottomBarConfigUtils.setBottomBarUserEnabled(false);
        assertTrue(BottomBarConfigUtils.shouldShowSettingsToggle(mContext));
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
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_ValidNtp() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertTrue(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_Incognito() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNtpScrollOffEnabled_BottomBarDisabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":ntp_scroll_off_enabled/true")
    public void testIsNtpScrollOffEnabled_KillSwitchEnabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertTrue(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":ntp_scroll_off_enabled/false")
    public void testIsNtpScrollOffEnabled_KillSwitchDisabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");

        assertFalse(BottomBarConfigUtils.isNtpScrollOffEnabled(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testAlwaysUseFilledIcon_DefaultTrue() {
        assertTrue(BottomBarConfigUtils.alwaysUseFilledIcon());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":always_use_filled_glic_icon/false")
    public void testAlwaysUseFilledIcon_FalseParam() {
        assertFalse(BottomBarConfigUtils.alwaysUseFilledIcon());
    }

    @Test
    public void testShouldForceBothConstraints_NullInputs() {
        assertFalse(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(null, mContext));
        assertFalse(BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, null));
        assertFalse(BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(null, null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_RegularNtp() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        when(mTab.isNativePage()).thenReturn(true);

        assertTrue(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_IncognitoNtp() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        when(mTab.isNativePage()).thenReturn(true);

        // Should return true for incognito NTP even though scroll-off is disabled.
        assertTrue(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_NormalPage() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);

        assertFalse(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_BottomBarDisabled() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        when(mTab.isNativePage()).thenReturn(true);

        assertFalse(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldForceBothConstraints_InternalScheme() {
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        assertTrue(
                BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(mTab, mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testIsGlicButtonEnabled() {
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());

        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertFalse(BottomBarConfigUtils.isGlicButtonEnabled());

        BottomBarConfigUtils.setGlicButtonEnabled(true);
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testIsGlicButtonEnabled_ToggleShown() {
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertFalse(BottomBarConfigUtils.isGlicButtonEnabled());
        BottomBarConfigUtils.setGlicButtonEnabled(true);
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false")
    public void testIsGlicButtonEnabled_ToggleNotShown() {
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertTrue(BottomBarConfigUtils.isGlicButtonEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testIsGlicSettingToggleParamEnabled_TrueParam() {
        assertTrue(BottomBarConfigUtils.isGlicSettingToggleParamEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false")
    public void testIsGlicSettingToggleParamEnabled_FalseParam() {
        assertFalse(BottomBarConfigUtils.isGlicSettingToggleParamEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testGetBottomBarHeightDp_DefaultBaseline() {
        assertEquals(
                BottomBarConfigUtils.DEFAULT_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/56")
    public void testGetBottomBarHeightDp_ToolbarMatch56() {
        assertEquals(56, BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/48")
    public void testGetBottomBarHeightDp_CompactAndroid48() {
        assertEquals(48, BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/44")
    public void testGetBottomBarHeightDp_ClampedBelowMin_Returns48() {
        assertEquals(
                BottomBarConfigUtils.MIN_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/72")
    public void testGetBottomBarHeightDp_ClampedAboveMax_Returns60() {
        assertEquals(
                BottomBarConfigUtils.MAX_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bottom_bar_height_dp/0")
    public void testGetBottomBarHeightDp_ZeroFallback_ReturnsDefault60() {
        assertEquals(
                BottomBarConfigUtils.DEFAULT_BOTTOM_BAR_HEIGHT_DP,
                BottomBarConfigUtils.getBottomBarHeightDp());
    }

    @Test
    public void testIsNtp() {
        assertFalse(BottomBarConfigUtils.isNtp(null));

        when(mTab.getNativePage()).thenReturn(null);
        assertFalse(BottomBarConfigUtils.isNtp(mTab));

        NativePage nativePage = mock(NativePage.class);
        when(nativePage.getHost()).thenReturn(UrlConstants.NTP_HOST);
        when(mTab.getNativePage()).thenReturn(nativePage);

        when(mTab.isOffTheRecord()).thenReturn(false);
        assertTrue(BottomBarConfigUtils.isNtp(mTab));

        when(mTab.isOffTheRecord()).thenReturn(true);
        assertTrue(BottomBarConfigUtils.isNtp(mTab));
    }
}
