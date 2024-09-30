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
@EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
public class EdgeToEdgeFieldTrialUnitTest {

    @Before
    public void setup() {
        EdgeToEdgeFieldTrial.clearInstanceForTesting();
    }

    // No provided override, the fieldtrial will use the default.
    @Test
    @Config(sdk = 30)
    public void noOverrides_meetMinVersion() {
        assertTrue(
                "Default manufacturer has min version override as 30.",
                EdgeToEdgeFieldTrial.getInstance().isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 29)
    public void noOverrides_notMeetMinVersion() {
        assertFalse(
                "Default manufacturer has min version override as 30.",
                EdgeToEdgeFieldTrial.getInstance().isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 30)
    public void overrides_notMeetMinVersion() {
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_LIST.setForTesting("foo,bar");
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_MIN_VERSIONS.setForTesting("31,32");
        var instance = EdgeToEdgeFieldTrial.getInstance();
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
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_LIST.setForTesting("foo,bar");
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_MIN_VERSIONS.setForTesting("31,32");
        var instance = EdgeToEdgeFieldTrial.getInstance();
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
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_LIST.setForTesting("foo,bar");
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_MIN_VERSIONS.setForTesting("31,32");
        var instance = EdgeToEdgeFieldTrial.getInstance();
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
    @Config(sdk = 28)
    public void testInvalidInputs_unevenLength() {
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_LIST.setForTesting("foobar");
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_MIN_VERSIONS.setForTesting("1,2");
        ShadowBuild.setManufacturer("foobar");
        assertFalse(
                "Invalid override is ignored.",
                EdgeToEdgeFieldTrial.getInstance().isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 28)
    public void testInvalidInputs_unevenLength_2() {
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_LIST.setForTesting("foo,bar");
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_MIN_VERSIONS.setForTesting("1");
        ShadowBuild.setManufacturer("foo");
        assertFalse(
                "Invalid override is ignored.",
                EdgeToEdgeFieldTrial.getInstance().isEnabledForManufacturerVersion());
    }

    @Test
    @Config(sdk = 28)
    public void testInvalidInputs_versionInvalid() {
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_LIST.setForTesting("foo,bar");
        EdgeToEdgeUtils.E2E_FIELD_TRIAL_OEM_MIN_VERSIONS.setForTesting("1,a");
        ShadowBuild.setManufacturer("foo");
        assertFalse(
                "Invalid override is ignored.",
                EdgeToEdgeFieldTrial.getInstance().isEnabledForManufacturerVersion());
    }
}
