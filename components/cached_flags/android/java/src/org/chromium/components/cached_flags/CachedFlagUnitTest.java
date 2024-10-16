// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import static org.chromium.base.test.util.BaseFlagTestRule.A_OFF_B_OFF;
import static org.chromium.base.test.util.BaseFlagTestRule.A_OFF_B_ON;
import static org.chromium.base.test.util.BaseFlagTestRule.A_ON_B_OFF;
import static org.chromium.base.test.util.BaseFlagTestRule.A_ON_B_ON;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_A;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_B;
import static org.chromium.base.test.util.BaseFlagTestRule.assertIsEnabledMatches;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.cached_flags.ValuesOverridden;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;

import java.util.Arrays;

/** Unit Tests for {@link CachedFlag}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedFlagUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @After
    public void tearDown() {
        ValuesReturned.clearForTesting();
        ValuesOverridden.removeOverrides();
    }

    @Test(expected = AssertionError.class)
    public void testDuplicateFeature_throwsException() {
        new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A, true);
        new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A, true);
    }

    @Test
    public void testNativeInitialized_getsFromChromeFeatureList() {
        CachedFlag featureA = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A, false);
        CachedFlag featureB = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_B, false);

        // Cache native flags, meaning values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_OFF_B_ON);
        CachedFlagUtils.cacheNativeFlags(Arrays.asList(featureA, featureB));

        // Assert {@link CachedFeatureFlags} uses the values from {@link ChromeFeatureList}.
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);
    }

    @Test
    public void testNativeNotInitializedNotCached_useDefault() {
        CachedFlag featureA = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A, true);
        CachedFlag featureB = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_B, false);

        // Do not cache values from native. There are no values stored in prefs either.
        FeatureList.setTestFeatures(A_OFF_B_ON);

        // Query the flags to make sure the default values are returned.
        assertIsEnabledMatches(A_ON_B_OFF, featureA, featureB);

        // Now do cache the values from ChromeFeatureList.
        CachedFlagUtils.cacheNativeFlags(Arrays.asList(featureA, featureB));

        // Verify that {@link CachedFlag} returns consistent values in the same run.
        assertIsEnabledMatches(A_ON_B_OFF, featureA, featureB);
    }

    @Test
    public void testNativeNotInitializedPrefsCached_getsFromPrefs() {
        CachedFlag featureA = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A, false);
        CachedFlag featureB = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_B, false);

        // Cache native flags, meaning values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_OFF_B_ON);
        CachedFlagUtils.cacheNativeFlags(Arrays.asList(featureA, featureB));
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);

        // Pretend the app was restarted. The SharedPrefs should remain.
        ValuesReturned.clearForTesting();
        ValuesOverridden.removeOverrides();

        // Simulate ChromeFeatureList retrieving new, different values for the flags.
        FeatureList.setTestFeatures(A_ON_B_ON);

        // Do not cache new values, but query the flags to make sure the values stored to prefs
        // are returned. Neither the defaults (false/false) or the ChromeFeatureList values
        // (true/true) should be returned.
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);

        // Now do cache the values from ChromeFeatureList.
        CachedFlagUtils.cacheNativeFlags(Arrays.asList(featureA, featureB));

        // Verify that {@link CachedFlag} returns consistent values in the same run.
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);

        // Pretend the app was restarted again.
        ValuesReturned.clearForTesting();
        ValuesOverridden.removeOverrides();

        // The SharedPrefs should retain the latest values.
        assertIsEnabledMatches(A_ON_B_ON, featureA, featureB);
    }

    @Test
    public void testSetForTesting_returnsForcedValue() {
        CachedFlag featureA = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A, false);
        CachedFlag featureB = new CachedFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_B, false);

        // Do not cache values from native. There are no values stored in prefs either.
        // Query the flags to make sure the default values are returned.
        assertIsEnabledMatches(A_OFF_B_OFF, featureA, featureB);

        // Force a feature flag.
        featureA.setForTesting(true);

        // Verify that the forced value is returned.
        assertIsEnabledMatches(A_ON_B_OFF, featureA, featureB);

        // Remove the forcing.
        featureA.setForTesting(null);

        // Verify that the forced value is not returned anymore.
        assertIsEnabledMatches(A_OFF_B_OFF, featureA, featureB);
    }
}
