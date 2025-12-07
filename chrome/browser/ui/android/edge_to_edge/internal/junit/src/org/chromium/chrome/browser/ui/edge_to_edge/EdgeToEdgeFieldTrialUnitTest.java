// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowBuild;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowBuild.class)
public class EdgeToEdgeFieldTrialUnitTest {

    @Before
    public void setup() {
        EdgeToEdgeFieldTrialImpl.clearInstanceForTesting();
    }

    // No provided override, the fieldtrial will use the default.
    @Test
    @Config(sdk = 30)
    public void noOverrides_meetMinVersion() {
        assertTrue(
                "Default manufacturer has min version override as 30.",
                EdgeToEdgeFieldTrialImpl.getBottomChinOverrides()
                        .isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 29)
    public void noOverrides_notMeetMinVersion() {
        assertFalse(
                "Default manufacturer has min version override as 30.",
                EdgeToEdgeFieldTrialImpl.getBottomChinOverrides()
                        .isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 30)
    public void overrides_notMeetMinVersion() {
        ChromeFeatureList.sEdgeToEdgeBottomChinOemList.setForTesting("foo,bar");
        ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.setForTesting("31,32");
        var instance = EdgeToEdgeFieldTrialImpl.getBottomChinOverrides();
        assertTrue(
                "Default have no min version override.",
                instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("foo");
        assertFalse(
                "foo has min version override as 31.", instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("bar");
        assertFalse(
                "bar has min version override as 32.", instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("baz");
        assertTrue(
                "baz is not on the override list and should use default as 30.",
                instance.isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 31)
    public void overrides_someMeetMinVersion() {
        ChromeFeatureList.sEdgeToEdgeBottomChinOemList.setForTesting("foo,bar");
        ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.setForTesting("31,32");
        var instance = EdgeToEdgeFieldTrialImpl.getBottomChinOverrides();
        assertTrue(
                "Default have no min version override.",
                instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("foo");
        assertTrue(
                "foo has min version override as 31.", instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("bar");
        assertFalse(
                "bar has min version override as 32.", instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("baz");
        assertTrue(
                "baz is not on the override list and should use default as 30.",
                instance.isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 32)
    public void overrides_meetMinVersion() {
        ChromeFeatureList.sEdgeToEdgeBottomChinOemList.setForTesting("foo,bar");
        ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.setForTesting("31,32");
        var instance = EdgeToEdgeFieldTrialImpl.getBottomChinOverrides();
        assertTrue(
                "Default have no min version override.",
                instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("foo");
        assertTrue(
                "foo has min version override as 31.", instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("bar");
        assertTrue(
                "bar has min version override as 32.", instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("baz");
        assertTrue(
                "baz is not on the override list and should use default as 30.",
                instance.isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 32)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void override_e2eEverywhere() {
        ChromeFeatureList.sEdgeToEdgeEverywhereOemList.setForTesting("foo,bar");
        ChromeFeatureList.sEdgeToEdgeEverywhereOemMinVersions.setForTesting("31,32");
        var instance = EdgeToEdgeFieldTrialImpl.getEverywhereOverrides();
        assertTrue(
                "Default have no min version override.",
                instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("foo");
        assertTrue(
                "foo has min version override as 31.", instance.isEnabledForManufacturerVersion());

        instance.resetCacheForTesting();
        ShadowBuild.setManufacturer("bar");
        assertTrue(
                "bar has min version override as 32.", instance.isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 31)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void override_e2eEverywhereAndBottomChin() {
        ChromeFeatureList.sEdgeToEdgeBottomChinOemList.setForTesting("foo");
        ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.setForTesting("32");

        ChromeFeatureList.sEdgeToEdgeEverywhereOemList.setForTesting("foo");
        ChromeFeatureList.sEdgeToEdgeEverywhereOemMinVersions.setForTesting("30");
        var bottomChinOverride = EdgeToEdgeFieldTrialImpl.getBottomChinOverrides();
        var everywhereOverrides = EdgeToEdgeFieldTrialImpl.getEverywhereOverrides();
        assertTrue(
                "Default have no min version override for bottom chin. Use 30.",
                bottomChinOverride.isEnabledForManufacturerVersion());
        assertTrue(
                "Default have no min version override for e2e everywhere. Use 30.",
                everywhereOverrides.isEnabledForManufacturerVersion());

        bottomChinOverride.resetCacheForTesting();
        everywhereOverrides.resetCacheForTesting();

        ShadowBuild.setManufacturer("foo");
        assertFalse(
                "foo has min version override as 32 for bottom chin.",
                bottomChinOverride.isEnabledForManufacturerVersion());
        assertTrue(
                "foo has min version override as 30 for everywhere.",
                everywhereOverrides.isEnabledForManufacturerVersion());
    }

    @Test
    public void testInvalidInputs_unevenLength() {
        ChromeFeatureList.sEdgeToEdgeBottomChinOemList.setForTesting("foobar");
        ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.setForTesting("1,2");
        ShadowBuild.setManufacturer("foobar");
        assertFalse(
                "Invalid override is ignored.",
                EdgeToEdgeFieldTrialImpl.getBottomChinOverrides()
                        .isEnabledForManufacturerVersion());
    }

    @Test
    public void testInvalidInputs_unevenLength_2() {
        ChromeFeatureList.sEdgeToEdgeBottomChinOemList.setForTesting("foo,bar");
        ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.setForTesting("1");
        ShadowBuild.setManufacturer("foo");
        assertFalse(
                "Invalid override is ignored.",
                EdgeToEdgeFieldTrialImpl.getBottomChinOverrides()
                        .isEnabledForManufacturerVersion());
    }

    @Test
    public void testInvalidInputs_versionInvalid() {
        ChromeFeatureList.sEdgeToEdgeBottomChinOemList.setForTesting("foo,bar");
        ChromeFeatureList.sEdgeToEdgeBottomChinOemMinVersions.setForTesting("1,a");
        ShadowBuild.setManufacturer("foo");
        assertFalse(
                "Invalid override is ignored.",
                EdgeToEdgeFieldTrialImpl.getBottomChinOverrides()
                        .isEnabledForManufacturerVersion());
    }
}
