// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.components.search_engines.TemplateUrlService;

/** Unit tests for {@link BottomBarActionEligibility}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarActionEligibilityUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private TemplateUrlService mTemplateUrlService;

    private String mCountry = "";

    @Before
    public void setUp() {
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        // Setup the country supplier to return our local member variable.
        BottomBarActionEligibility.setCountrySupplier(() -> mCountry);
        // By default, mock the DSE to be Google.
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
    }

    @org.junit.After
    public void tearDown() {
        BottomBarActionEligibility.setCountrySupplier(null);
    }

    @Test
    public void testGetEligibleExtraAction_GlicEnabled_US() {
        mCountry = "us";
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_GlicEnabled_NonUS() {
        mCountry = "au"; // Australia (Not allowed for Glic, but allowed for AIM)
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        // Should not return GLIC because country is not US, but should fall back to AI Mode if DSE
        // is Google.
        assertEquals(ActionId.AI_MODE, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_GlicSoonCountry() {
        mCountry = "in"; // India (Soon country -> Should show nothing)
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_AiMode_Eligible() {
        mCountry = "au"; // Australia (AIM allowed)
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        assertEquals(ActionId.AI_MODE, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_AiMode_DseNotGoogle() {
        mCountry = "au"; // Australia (AIM allowed)
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(false);
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_OtherCountry() {
        mCountry = "fr"; // France (Not in any allowlist)
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_BlockedCountries() {
        // France and China are in the blocklist.
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        mCountry = "fr";
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));

        mCountry = "cn";
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    public void testGetEligibleExtraAction_NullProfile() {
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bypass_glic_geofencing/true")
    public void testGetEligibleExtraAction_GlicBypassGeofencing() {
        // France (Blocked/not allowed for GLIC).
        mCountry = "fr";
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        // Should return GLIC even in France!
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bypass_aim_geofencing/true")
    public void testGetEligibleExtraAction_AiModeBypassGeofencing() {
        // France (Not allowed for AI Mode).
        mCountry = "fr";
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        // Should return AI_MODE even in France!
        assertEquals(ActionId.AI_MODE, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testGetEligibleExtraAction_GlicButtonDisabledByUser() {
        mCountry = "us";
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));

        BottomBarConfigUtils.setGlicButtonEnabled(true);
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testGetEligibleExtraAction_GlicButtonDisabledByUser_ToggleShown() {
        mCountry = "us";
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getEligibleExtraAction(mProfile));
        BottomBarConfigUtils.setGlicButtonEnabled(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false")
    public void testGetEligibleExtraAction_GlicButtonDisabledByUser_ToggleNotShown() {
        mCountry = "us";
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getEligibleExtraAction(mProfile));
        BottomBarConfigUtils.setGlicButtonEnabled(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testShouldShowBottomBarGlicSetting_ToggleParamTrue() {
        mCountry = "us";
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
        assertTrue(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false")
    public void testShouldShowBottomBarGlicSetting_ToggleParamFalse() {
        mCountry = "us";
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
        assertFalse(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(mProfile));
    }
}
