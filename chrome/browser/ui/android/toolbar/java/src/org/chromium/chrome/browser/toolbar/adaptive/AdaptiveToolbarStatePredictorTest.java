// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor.UiState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

/** Unit tests for the {@code AdaptiveToolbarStatePredictor} */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
        ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
public class AdaptiveToolbarStatePredictorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private AndroidPermissionDelegate mAndroidPermissionDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(true);
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
    }

    @After
    public void tearDown() {
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(null);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
    public void testDisableFeature() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);
        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(
                true, AdaptiveToolbarButtonVariant.VOICE, true, AdaptiveToolbarButtonVariant.SHARE);
        UiState expected = new UiState(false, AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarButtonVariant.UNKNOWN, AdaptiveToolbarButtonVariant.UNKNOWN);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testManualOverride() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);

        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(
                true, AdaptiveToolbarButtonVariant.VOICE, true, AdaptiveToolbarButtonVariant.SHARE);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarButtonVariant.VOICE, AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectFinchDefaultWhenNotUsingSegmentation() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(true);

        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(true,
                AdaptiveToolbarButtonVariant.UNKNOWN, true, AdaptiveToolbarButtonVariant.VOICE);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectValidSegmentWhenSegmentationSucceeds() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);

        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(true,
                AdaptiveToolbarButtonVariant.UNKNOWN, true, AdaptiveToolbarButtonVariant.VOICE);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectValidSegmentWhenVoiceDisabled() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);

        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(false);
        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(true,
                AdaptiveToolbarButtonVariant.UNKNOWN, true, AdaptiveToolbarButtonVariant.VOICE);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testExpectFinchDefaultWhenSegmentationFails() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);

        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(true,
                AdaptiveToolbarButtonVariant.UNKNOWN, true, AdaptiveToolbarButtonVariant.UNKNOWN);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testToolbarSettingsToggleDisabled() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);

        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(false,
                AdaptiveToolbarButtonVariant.VOICE, true, AdaptiveToolbarButtonVariant.SHARE);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarButtonVariant.VOICE, AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testDisableUi() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);
        AdaptiveToolbarFeatures.setDisableUiForTesting(true);

        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(true,
                AdaptiveToolbarButtonVariant.UNKNOWN, true, AdaptiveToolbarButtonVariant.VOICE);
        UiState expected = new UiState(false, AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testWithShowUiOnlyAfterReady() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);

        // Configure to not show if backend is not ready.
        AdaptiveToolbarFeatures.setShowUiOnlyAfterReadyForTesting(true);

        // Before backend is ready.
        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(true,
                AdaptiveToolbarButtonVariant.UNKNOWN, false, AdaptiveToolbarButtonVariant.VOICE);
        UiState expected = new UiState(false, AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));

        // Backend isn't ready and doesn't give a valid segment.
        statePredictor = buildStatePredictor(true, AdaptiveToolbarButtonVariant.UNKNOWN, false,
                AdaptiveToolbarButtonVariant.UNKNOWN);
        expected = new UiState(false, AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));

        // After backend is ready.
        statePredictor = buildStatePredictor(true, AdaptiveToolbarButtonVariant.UNKNOWN, true,
                AdaptiveToolbarButtonVariant.VOICE);
        expected = new UiState(true, AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testWithoutShowUiOnlyAfterReady() {
        AdaptiveToolbarFeatures.setDefaultSegmentForTesting(AdaptiveToolbarFeatures.SHARE);
        AdaptiveToolbarFeatures.setIgnoreSegmentationResultsForTesting(false);

        // Configure to show even if backend is not ready.
        AdaptiveToolbarFeatures.setShowUiOnlyAfterReadyForTesting(false);

        // Before backend is ready.
        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(true,
                AdaptiveToolbarButtonVariant.UNKNOWN, false, AdaptiveToolbarButtonVariant.VOICE);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.VOICE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));

        // Backend isn't ready and doesn't give a valid segment.
        statePredictor = buildStatePredictor(true, AdaptiveToolbarButtonVariant.UNKNOWN, false,
                AdaptiveToolbarButtonVariant.UNKNOWN);
        expected = new UiState(true, AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarButtonVariant.SHARE);
        statePredictor.recomputeUiState(verifyResultCallback(expected));
    }

    @Test
    @SmallTest
    public void testSegmentIdToAdaptiveToolbarButtonVariantConversion() {
        Assert.assertEquals(AdaptiveToolbarButtonVariant.NEW_TAB,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_SHARE));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromSegmentId(
                        SegmentId.OPTIMIZATION_TARGET_UNKNOWN));
    }

    private AdaptiveToolbarStatePredictor buildStatePredictor(boolean toolbarSettingsToggleEnabled,
            Integer manualOverride, boolean isReady, Integer segmentationResult) {
        return new AdaptiveToolbarStatePredictor(mAndroidPermissionDelegate) {
            @Override
            int readManualOverrideFromPrefs() {
                return manualOverride;
            }

            @Override
            boolean readToolbarToggleStateFromPrefs() {
                return toolbarSettingsToggleEnabled;
            }

            @Override
            public void readFromSegmentationPlatform(Callback<Pair<Boolean, Integer>> callback) {
                callback.onResult(new Pair<>(isReady, segmentationResult));
            }
        };
    }

    private Callback<UiState> verifyResultCallback(UiState expected) {
        return result -> {
            Assert.assertEquals("canShowUi doesn't match", expected.canShowUi, result.canShowUi);
            Assert.assertEquals("toolbarButtonState doesn't match", expected.toolbarButtonState,
                    result.toolbarButtonState);
            Assert.assertEquals("preferenceSelection doesn't match", expected.preferenceSelection,
                    result.preferenceSelection);
            Assert.assertEquals("autoButtonCaption doesn't match", expected.autoButtonCaption,
                    result.autoButtonCaption);
        };
    }
}
