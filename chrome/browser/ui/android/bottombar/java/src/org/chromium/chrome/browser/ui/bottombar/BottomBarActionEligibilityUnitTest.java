// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.GlicIneligibilityReason;

/** Unit tests for {@link BottomBarActionEligibility}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarActionEligibilityUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    @Before
    public void setUp() {
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(null);
    }

    @Test
    public void testShouldShowBottomBarGlicSetting_NullProfile() {
        assertFalse(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(null));
    }

    @Test
    public void testGetCandidateExtraAction_GlicEnabled() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getCandidateExtraAction(mProfile));
    }

    @Test
    public void testGetCandidateExtraAction_GlicDisabled() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getCandidateExtraAction(mProfile));
    }

    @Test
    public void testGetCandidateExtraAction_NullProfile() {
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getCandidateExtraAction(/* profile= */ null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testGetCandidateExtraAction_GlicButtonDisabledByUser_Unmanaged_ReturnsActionNone() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(false);

        // When disabled by user and not policy-enforced -> returns ACTION_NONE.
        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ false);
        assertEquals(
                BottomBarActionEligibility.ACTION_NONE,
                BottomBarActionEligibility.getCandidateExtraAction(mProfile));
        // But cached candidate is still GLIC.
        assertEquals(
                Integer.valueOf(ActionId.GLIC),
                BottomBarActionEligibility.getCachedCandidateExtraAction());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void
            testGetCandidateExtraAction_GlicButtonDisabledByUser_PolicyEnforced_ForceShowsGlic() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(true);

        // When policy enforces GLIC, it force-shows even if user setting was toggled off.
        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ false);
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getCandidateExtraAction(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testGetCandidateExtraAction_GlicButtonEnabledByUser_ReturnsGlic() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(false);

        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ true);
        assertEquals(ActionId.GLIC, BottomBarActionEligibility.getCandidateExtraAction(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testShouldShowBottomBarGlicSetting_ToggleParamTrue_GlicCandidate() {
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(ActionId.GLIC);
        assertTrue(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testShouldShowBottomBarGlicSetting_NoCachedCandidate_ReturnsFalse() {
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
        // Candidate not resolved yet (null).
        assertFalse(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testShouldShowBottomBarGlicSetting_ActionNoneCandidate_ReturnsFalse() {
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(
                BottomBarActionEligibility.ACTION_NONE);
        assertFalse(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/false")
    public void testShouldShowBottomBarGlicSetting_ToggleParamFalse() {
        when(mGlicEnablingJniMock.shouldShowSettingsPage(any())).thenReturn(true);
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(ActionId.GLIC);
        assertFalse(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(mProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testShouldShowBottomBarGlicSetting_IncognitoProfile_UsesOriginalProfile() {
        Profile incognitoProfile = org.mockito.Mockito.mock(Profile.class);
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);
        when(incognitoProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mGlicEnablingJniMock.shouldShowSettingsPage(eq(mProfile))).thenReturn(true);
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(ActionId.GLIC);

        assertTrue(BottomBarActionEligibility.shouldShowBottomBarGlicSetting(incognitoProfile));
    }

    @Test
    public void testGetCandidateExtraAction_IncognitoProfile_UsesOriginalProfile() {
        Profile incognitoProfile = org.mockito.Mockito.mock(Profile.class);
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);
        when(incognitoProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mGlicEnablingJniMock.isEnabledForProfile(eq(mProfile))).thenReturn(true);

        assertEquals(
                ActionId.GLIC,
                BottomBarActionEligibility.getCandidateExtraAction(incognitoProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testGetCandidateExtraAction_DoesNotRecordIneligibilityReason() {
        var noRecordWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.BottomBar.Glic.IneligibilityReason")
                        .build();

        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        BottomBarActionEligibility.getCandidateExtraAction(mProfile);

        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(false);
        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ false);
        BottomBarActionEligibility.getCandidateExtraAction(mProfile);

        noRecordWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testRecordGlicIneligibilityReasonIfNeeded() {
        // 1. GLIC profile ineligible -> GLIC ProfileIneligible recorded.
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        var glicProfileIneligibleWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.Glic.IneligibilityReason",
                        GlicIneligibilityReason.PROFILE_INELIGIBLE);
        BottomBarActionEligibility.recordGlicIneligibilityReasonIfNeeded(mProfile);
        glicProfileIneligibleWatcher.assertExpected();

        // 2. User disabled in settings -> GLIC UserDisabledInSettings recorded.
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(false);
        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ false);
        var glicUserDisabledWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.Glic.IneligibilityReason",
                        GlicIneligibilityReason.USER_DISABLED_IN_SETTINGS);
        BottomBarActionEligibility.recordGlicIneligibilityReasonIfNeeded(mProfile);
        glicUserDisabledWatcher.assertExpected();

        // 3. GLIC eligible -> no ineligibility reason recorded.
        BottomBarConfigUtils.setGlicButtonEnabled(/* enabled= */ true);
        var glicEligibleWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.BottomBar.Glic.IneligibilityReason")
                        .build();
        BottomBarActionEligibility.recordGlicIneligibilityReasonIfNeeded(mProfile);
        glicEligibleWatcher.assertExpected();
    }
}
