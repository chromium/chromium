// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link AdaptiveToolbarFeatures}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveToolbarFeaturesTest {
    private TestValues mTestValues = new TestValues();

    @Before
    public void setUp() {
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
                AdaptiveToolbarFeatures.VARIATION_PARAM_MIN_VERSION, testMinVersion.toString());

        assertTrue(AdaptiveToolbarFeatures.isCustomizationEnabled());
    }

    @Test
    public void testEnabledWhenMinVersionEqual() {
        mTestValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        Integer testMinVersion = AdaptiveToolbarFeatures.VERSION;
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                AdaptiveToolbarFeatures.VARIATION_PARAM_MIN_VERSION, testMinVersion.toString());

        assertTrue(AdaptiveToolbarFeatures.isCustomizationEnabled());
    }

    @Test
    public void testDisabledWhenMinVersionGreater() {
        mTestValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        Integer testMinVersion = AdaptiveToolbarFeatures.VERSION + 1;
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2,
                AdaptiveToolbarFeatures.VARIATION_PARAM_MIN_VERSION, testMinVersion.toString());

        assertFalse(AdaptiveToolbarFeatures.isCustomizationEnabled());
    }
}
