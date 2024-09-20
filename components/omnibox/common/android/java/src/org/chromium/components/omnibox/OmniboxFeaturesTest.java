// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/** Tests for {@link OmniboxFeatures}. */
@RunWith(Enclosed.class)
public class OmniboxFeaturesTest {

    /** Parameterized tests for {@link OmniboxFeatures#shouldRetainOmniboxOnFocus()}. */
    @RunWith(ParameterizedRobolectricTestRunner.class)
    public static class ShouldRetainOmniboxOnFocusTest {

        /** Returns all possible combinations for test parameterization. */
        @Parameters(name = "{0}, {1}, {2}")
        public static Collection<Object[]> data() {
            Collection<Object[]> data = new ArrayList<>();
            for (boolean deviceFormFactorIsTablet : List.of(true, false)) {
                for (boolean deviceInputSupportsAlphabeticKeyboard : List.of(true, false)) {
                    for (boolean deviceInputSupportsPrecisionPointer : List.of(true, false)) {
                        data.add(
                                new Object[] {
                                    deviceFormFactorIsTablet,
                                    deviceInputSupportsAlphabeticKeyboard,
                                    deviceInputSupportsPrecisionPointer
                                });
                    }
                }
            }
            return data;
        }

        @Rule(order = -2)
        public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

        /** Whether the device form factor is a tablet. */
        @Parameter(0)
        public boolean mDeviceFormFactorIsTablet;

        /** Whether device input supports an alphabetic keyboard. */
        @Parameter(1)
        public boolean mDeviceInputSupportsAlphabeticKeyboard;

        /** Whether device input supports a precision pointer. */
        @Parameter(2)
        public boolean mDeviceInputSupportsPrecisionPointer;

        @Before
        public void setUp() {
            DeviceFormFactor.setIsTabletForTesting(mDeviceFormFactorIsTablet);
            DeviceInput.setSupportsAlphabeticKeyboardForTesting(
                    mDeviceInputSupportsAlphabeticKeyboard);
            DeviceInput.setSupportsPrecisionPointerForTesting(mDeviceInputSupportsPrecisionPointer);
        }

        @Test
        @SmallTest
        public void testShouldRetainOmniboxOnFocus_withFeatureInDefaultState() {
            testShouldRetainOmniboxOnFocus(/* expectFeatureEnabled= */ false);
        }

        @Test
        @SmallTest
        @DisableFeatures(OmniboxFeatureList.RETAIN_OMNIBOX_ON_FOCUS)
        public void testShouldRetainOmniboxOnFocus_withFeatureExplicitlyDisabled() {
            testShouldRetainOmniboxOnFocus(/* expectFeatureEnabled= */ false);
        }

        @Test
        @SmallTest
        @EnableFeatures(OmniboxFeatureList.RETAIN_OMNIBOX_ON_FOCUS)
        public void testShouldRetainOmniboxOnFocus_withFeatureExplicitlyEnabled() {
            testShouldRetainOmniboxOnFocus(/* expectFeatureEnabled= */ true);
        }

        private void testShouldRetainOmniboxOnFocus(boolean expectFeatureEnabled) {
            boolean featureEnabled = OmniboxFeatures.sRetainOmniboxOnFocus.isEnabled();
            Assert.assertEquals(expectFeatureEnabled, featureEnabled);

            boolean expectRetainOmniboxOnFocus =
                    featureEnabled
                            && mDeviceFormFactorIsTablet
                            && mDeviceInputSupportsAlphabeticKeyboard
                            && mDeviceInputSupportsPrecisionPointer;

            Assert.assertEquals(
                    expectRetainOmniboxOnFocus, OmniboxFeatures.shouldRetainOmniboxOnFocus());
        }
    }
}
