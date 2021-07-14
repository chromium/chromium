// Copyright 2021 The Chromium Authors. All rights reserved.
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
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor.UiState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.optimization_guide.proto.ModelsProto.OptimizationTarget;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for the {@code AdaptiveToolbarStatePredictor} */
@Config(manifest = Config.NONE,
        shadows = {AdaptiveToolbarStatePredictorTest.ShadowChromeFeatureList.class})
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR,
        ChromeFeatureList.SHARE_BUTTON_IN_TOP_TOOLBAR,
        ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
public class AdaptiveToolbarStatePredictorTest {
    // TODO(crbug.com/1199025): Remove this shadow.
    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static final Map<String, String> sParamValues = new HashMap<>();

        @Implementation
        public static String getFieldTrialParamByFeature(String feature, String paramKey) {
            Assert.assertTrue(ChromeFeatureList.isEnabled(feature));
            return sParamValues.getOrDefault(paramKey, "");
        }

        public static void reset() {
            sParamValues.clear();
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Before
    public void setUp() {
        ShadowChromeFeatureList.reset();
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
    }

    @After
    public void tearDown() {
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
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
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testWorksWithDataCollectionFeatureFlag() {
        ShadowChromeFeatureList.sParamValues.put("mode", "always-voice");
        AdaptiveToolbarStatePredictor statePredictor = buildStatePredictor(
                true, AdaptiveToolbarButtonVariant.VOICE, true, AdaptiveToolbarButtonVariant.SHARE);
        UiState expected = new UiState(true, AdaptiveToolbarButtonVariant.VOICE,
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
    public void testOptimizationTargetToAdaptiveToolbarButtonVariantConversion() {
        Assert.assertEquals(AdaptiveToolbarButtonVariant.NEW_TAB,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromOptimizationTarget(
                        OptimizationTarget.OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromOptimizationTarget(
                        OptimizationTarget.OPTIMIZATION_TARGET_SEGMENTATION_SHARE));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.VOICE,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromOptimizationTarget(
                        OptimizationTarget.OPTIMIZATION_TARGET_SEGMENTATION_VOICE));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromOptimizationTarget(
                        OptimizationTarget.OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
        Assert.assertEquals(AdaptiveToolbarButtonVariant.UNKNOWN,
                AdaptiveToolbarStatePredictor.getAdaptiveToolbarButtonVariantFromOptimizationTarget(
                        OptimizationTarget.OPTIMIZATION_TARGET_UNKNOWN));
    }

    private AdaptiveToolbarStatePredictor buildStatePredictor(boolean toolbarSettingsToggleEnabled,
            Integer manualOverride, boolean isReady, Integer segmentationResult) {
        return new AdaptiveToolbarStatePredictor() {
            @Override
            int readManualOverrideFromPrefs() {
                return manualOverride;
            }

            @Override
            boolean readToolbarToggleStateFromPrefs() {
                return toolbarSettingsToggleEnabled;
            }

            @Override
            void readFromSegmentationPlatform(Callback<Pair<Boolean, Integer>> callback) {
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
