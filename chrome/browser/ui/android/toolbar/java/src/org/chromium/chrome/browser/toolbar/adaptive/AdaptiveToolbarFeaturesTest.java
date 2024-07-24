// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.List;

/** Unit tests for {@link AdaptiveToolbarFeatures}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveToolbarFeaturesTest {
    private Activity mActivity;
    private TestValues mTestValues = new TestValues();

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        FeatureList.setTestValues(mTestValues);
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
    }

    @Test
    public void testEnabledWhenMinVersionLesser() {
        mTestValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        Integer testMinVersion = AdaptiveToolbarFeatures.VERSION - 1;
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                AdaptiveToolbarFeatures.VARIATION_PARAM_MIN_VERSION,
                testMinVersion.toString());

        assertTrue(AdaptiveToolbarFeatures.isCustomizationEnabled());
    }

    @Test
    public void testEnabledWhenMinVersionEqual() {
        mTestValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        Integer testMinVersion = AdaptiveToolbarFeatures.VERSION;
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                AdaptiveToolbarFeatures.VARIATION_PARAM_MIN_VERSION,
                testMinVersion.toString());

        assertTrue(AdaptiveToolbarFeatures.isCustomizationEnabled());
    }

    @Test
    public void testDisabledWhenMinVersionGreater() {
        mTestValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        Integer testMinVersion = AdaptiveToolbarFeatures.VERSION + 1;
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                AdaptiveToolbarFeatures.VARIATION_PARAM_MIN_VERSION,
                testMinVersion.toString());

        assertFalse(AdaptiveToolbarFeatures.isCustomizationEnabled());
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    public void testGetTopSegmentationResultOnPhone() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.NEW_TAB);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithNTBOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.SHARE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                        AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.SHARE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithNoNTBOrBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.TRANSLATE, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.TRANSLATE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithOnlyNTBAndBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                        AdaptiveToolbarButtonVariant.NEW_TAB),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.UNKNOWN);
    }

    private void assertTopResult(
            List<Integer> segmentationResults,
            @AdaptiveToolbarButtonVariant int expectedTopResult) {
        assertEquals(
                "Top segmentation result is not as expected.",
                expectedTopResult,
                AdaptiveToolbarFeatures.getTopSegmentationResult(mActivity, segmentationResults));
    }
}
