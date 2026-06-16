// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor.UiState;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@code AdaptiveToolbarStatePredictor} */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
public class AdaptiveToolbarStatePredictorTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private AndroidPermissionDelegate mAndroidPermissionDelegate;
    @Mock private PrefService mPrefService;
    @Mock private GlicEnabling.Natives mGlicEnablingNatives;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(true);
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        UserPrefs.setPrefServiceForTesting(mPrefService);
        when(mPrefService.getBoolean(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(true);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingNatives);
        when(mGlicEnablingNatives.isEnabledForProfile(mProfile)).thenReturn(true);
    }

    @After
    public void tearDown() {
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
    }

    @Test
    @SmallTest
    public void testExpectTranslateFilteredWhenDisabled() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        // Disable translate.
        when(mPrefService.isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(false);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.TRANSLATE));

        // Translate should be filtered out, falling back to default (SHARE).
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectTranslateFilteredWhenDisabled_ManualOverride() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        // Disable translate.
        when(mPrefService.isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(false);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.TRANSLATE,
                        List.of(AdaptiveToolbarButtonVariant.SHARE));

        // Manual override for Translate should be filtered out, falling back to default (SHARE).
        // Preference selection should also fall back to AUTO.
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testDisableFeature() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.VOICE,
                        List.of(AdaptiveToolbarButtonVariant.SHARE));
        UiState expected =
                new UiState(
                        false,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.UNKNOWN)),
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        AdaptiveToolbarButtonVariant.UNKNOWN);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testManualOverride() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.VOICE,
                        List.of(AdaptiveToolbarButtonVariant.SHARE));
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.VOICE)),
                        AdaptiveToolbarButtonVariant.VOICE,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectValidSegmentWhenSegmentationSucceeds() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(
                                AdaptiveToolbarButtonVariant.VOICE,
                                AdaptiveToolbarButtonVariant.TRANSLATE));
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(
                                List.of(
                                        AdaptiveToolbarButtonVariant.VOICE,
                                        AdaptiveToolbarButtonVariant.TRANSLATE,
                                        AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectValidSegmentWhenVoiceDisabled() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(false);
        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.VOICE));
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectDefaultSegmentWhenSegmentationFails() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.UNKNOWN));
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testToolbarSettingsToggleDisabled() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        false,
                        AdaptiveToolbarButtonVariant.VOICE,
                        List.of(AdaptiveToolbarButtonVariant.SHARE));
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.UNKNOWN)),
                        AdaptiveToolbarButtonVariant.VOICE,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testGlicEnabled_GlicNotRecommendedInAuto() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.VOICE));

        // Glic should NOT be included in results when in AUTO.
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(
                                List.of(
                                        AdaptiveToolbarButtonVariant.VOICE,
                                        AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testGlicManualOverride() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.GLIC,
                        List.of(AdaptiveToolbarButtonVariant.VOICE));

        // Manual override of GLIC should be respected.
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.GLIC)),
                        AdaptiveToolbarButtonVariant.GLIC,
                        AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testGlicEnabled_ManualOverride() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.SHARE,
                        List.of(AdaptiveToolbarButtonVariant.VOICE));

        // Manual override should be respected even if Glic is enabled.
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.SHARE,
                        AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testWithoutShowUiOnlyAfterReady() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        // Before backend is ready.
        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.VOICE));
        UiState expected =
                new UiState(
                        true,
                        new ArrayList<>(
                                List.of(
                                        AdaptiveToolbarButtonVariant.VOICE,
                                        AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));

        // Backend isn't ready and doesn't give a valid segment.
        statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.UNKNOWN));
        expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testSegmentIdToAdaptiveToolbarButtonVariantConversion() {
        assertEquals(
                AdaptiveToolbarButtonVariant.NEW_TAB,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB));
        assertEquals(
                AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_SHARE));
        assertEquals(
                AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
        assertEquals(
                AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER));
        assertEquals(
                AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_UNKNOWN));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testNewTabEnabled() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.NEW_TAB));

        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(
                                List.of(
                                        AdaptiveToolbarButtonVariant.NEW_TAB,
                                        AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.NEW_TAB);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testNewTabFilteredWhenBottomBarEnabled() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);

        AdaptiveToolbarStatePredictor statePredictor =
                buildStatePredictor(
                        true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        List.of(AdaptiveToolbarButtonVariant.NEW_TAB));

        UiState expected =
                new UiState(
                        true,
                        new ArrayList<Integer>(List.of(AdaptiveToolbarButtonVariant.SHARE)),
                        AdaptiveToolbarButtonVariant.AUTO,
                        AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    private AdaptiveToolbarStatePredictor buildStatePredictor(
            boolean toolbarSettingsToggleEnabled,
            Integer manualOverride,
            List<Integer> segmentationResults) {
        return new AdaptiveToolbarStatePredictor(
                mActivity, mProfile, mAndroidPermissionDelegate, /* behavior= */ null) {
            @Override
            int readManualOverrideFromPrefs() {
                return manualOverride;
            }

            @Override
            boolean readToolbarToggleStateFromPrefs() {
                return toolbarSettingsToggleEnabled;
            }

            @Override
            public void readFromSegmentationPlatform(Callback<List<Integer>> callback) {
                callback.onResult(segmentationResults);
            }
        };
    }

    private Callback<UiState> verifyResultCallback(UiState expected) {
        return result -> {
            assertEquals("canShowUi doesn't match", expected.canShowUi, result.canShowUi);
            assertEquals(
                    "rankedToolbarButtonStates doesn't match",
                    expected.rankedToolbarButtonStates,
                    result.rankedToolbarButtonStates);
            assertEquals(
                    "preferenceSelection doesn't match",
                    expected.preferenceSelection,
                    result.preferenceSelection);
            assertEquals(
                    "autoButtonCaption doesn't match",
                    expected.autoButtonCaption,
                    result.autoButtonCaption);
        };
    }
}
