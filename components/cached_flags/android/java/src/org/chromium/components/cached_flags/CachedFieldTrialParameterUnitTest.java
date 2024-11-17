// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureMap;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;
import org.chromium.base.test.util.Features.EnableFeatures;

import java.util.List;

/** Unit Tests for {@link CachedFieldTrialParameter} and its subclasses. */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedFieldTrialParameterUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    private static final String FEATURE_A = "FeatureA";

    private static final String STRING_PARAM_NAME = "ParamString";
    private static final String STRING_PARAM_DEFAULT = "default";
    private static final String STRING_PARAM_NATIVE = "native";
    private static final String STRING_PARAM_TEST_OVERRIDE = "override";
    private static final String STRING_PARAM_BAD = "bad";

    private static final String BOOLEAN_PARAM_NAME = "ParamBoolean";
    private static final boolean BOOLEAN_PARAM_DEFAULT = false;
    private static final boolean BOOLEAN_PARAM_NATIVE = true;
    private static final boolean BOOLEAN_PARAM_TEST_OVERRIDE = true;
    private static final boolean BOOLEAN_PARAM_BAD = false;

    private static final String INT_PARAM_NAME = "ParamInt";
    private static final int INT_PARAM_DEFAULT = 1;
    private static final int INT_PARAM_NATIVE = 2;
    private static final int INT_PARAM_TEST_OVERRIDE = 3;
    private static final int INT_PARAM_BAD = 9;

    private static final String DOUBLE_PARAM_NAME = "ParamDouble";
    private static final double DOUBLE_PARAM_DEFAULT = 1.0;
    private static final double DOUBLE_PARAM_NATIVE = 2.0;
    private static final double DOUBLE_PARAM_TEST_OVERRIDE = 3.0;
    private static final double DOUBLE_PARAM_BAD = 9.0;

    // Different in that the native value will be "", which makes the default be cached.
    private static final String STRING2_PARAM_NAME = "ParamString2";
    private static final String STRING2_PARAM_DEFAULT = "default2";
    private static final String STRING2_PARAM_NATIVE = "";
    private static final String STRING2_PARAM_TEST_OVERRIDE = "override2";
    private static final String STRING2_PARAM_BAD = "bad2";

    @Mock private FeatureMap mFeatureMap;

    private StringCachedFieldTrialParameter mStringParam;
    private BooleanCachedFieldTrialParameter mBooleanParam;
    private IntCachedFieldTrialParameter mIntParam;
    private DoubleCachedFieldTrialParameter mDoubleParam;
    private StringCachedFieldTrialParameter mString2Param;

    private List<CachedFieldTrialParameter<?>> mParamsToCache;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mFeatureMap.getFieldTrialParamByFeature(FEATURE_A, STRING_PARAM_NAME))
                .thenReturn(STRING_PARAM_NATIVE);
        when(mFeatureMap.getFieldTrialParamByFeatureAsBoolean(
                        FEATURE_A, BOOLEAN_PARAM_NAME, BOOLEAN_PARAM_DEFAULT))
                .thenReturn(BOOLEAN_PARAM_NATIVE);
        when(mFeatureMap.getFieldTrialParamByFeatureAsDouble(
                        FEATURE_A, DOUBLE_PARAM_NAME, DOUBLE_PARAM_DEFAULT))
                .thenReturn(DOUBLE_PARAM_NATIVE);
        when(mFeatureMap.getFieldTrialParamByFeatureAsInt(
                        FEATURE_A, INT_PARAM_NAME, INT_PARAM_DEFAULT))
                .thenReturn(INT_PARAM_NATIVE);
        when(mFeatureMap.getFieldTrialParamByFeature(FEATURE_A, STRING2_PARAM_NAME))
                .thenReturn(STRING2_PARAM_NATIVE);

        mStringParam =
                new StringCachedFieldTrialParameter(
                        mFeatureMap, FEATURE_A, STRING_PARAM_NAME, STRING_PARAM_DEFAULT);
        mBooleanParam =
                new BooleanCachedFieldTrialParameter(
                        mFeatureMap, FEATURE_A, BOOLEAN_PARAM_NAME, BOOLEAN_PARAM_DEFAULT);
        mIntParam =
                new IntCachedFieldTrialParameter(
                        mFeatureMap, FEATURE_A, INT_PARAM_NAME, INT_PARAM_DEFAULT);
        mDoubleParam =
                new DoubleCachedFieldTrialParameter(
                        mFeatureMap, FEATURE_A, DOUBLE_PARAM_NAME, DOUBLE_PARAM_DEFAULT);
        mString2Param =
                new StringCachedFieldTrialParameter(
                        mFeatureMap, FEATURE_A, STRING2_PARAM_NAME, STRING2_PARAM_DEFAULT);
        mParamsToCache =
                List.of(mStringParam, mBooleanParam, mIntParam, mDoubleParam, mString2Param);
    }

    @Test
    public void testNativeNotInitializedNotCached_useDefault() {
        assertValuesAreDefault();
    }

    @Test
    public void testNativeInitialized_getsFromFeatureMap() {
        CachedFlagUtils.cacheFieldTrialParameters(mParamsToCache);
        assertValuesAreFromNative();
    }

    @Test
    public void testConsistency() {
        assertValuesAreDefault();
        CachedFlagUtils.cacheFieldTrialParameters(mParamsToCache);

        // Should still return the values previously returned
        assertValuesAreDefault();
    }

    @Test
    public void testNativeNotInitializedPrefsCached_getsFromPrefs() {
        // Cache to disk
        CachedFlagUtils.cacheFieldTrialParameters(mParamsToCache);

        // Simulate a second run
        ValuesReturned.clearForTesting();

        // Set different values in native which shouldn't be used
        when(mFeatureMap.getFieldTrialParamByFeature(FEATURE_A, STRING_PARAM_NAME))
                .thenReturn(STRING_PARAM_BAD);
        when(mFeatureMap.getFieldTrialParamByFeatureAsBoolean(
                        FEATURE_A, BOOLEAN_PARAM_NAME, BOOLEAN_PARAM_DEFAULT))
                .thenReturn(BOOLEAN_PARAM_BAD);
        when(mFeatureMap.getFieldTrialParamByFeatureAsDouble(
                        FEATURE_A, DOUBLE_PARAM_NAME, DOUBLE_PARAM_DEFAULT))
                .thenReturn(DOUBLE_PARAM_BAD);
        when(mFeatureMap.getFieldTrialParamByFeatureAsInt(
                        FEATURE_A, INT_PARAM_NAME, INT_PARAM_DEFAULT))
                .thenReturn(INT_PARAM_BAD);
        when(mFeatureMap.getFieldTrialParamByFeature(FEATURE_A, STRING2_PARAM_NAME))
                .thenReturn(STRING2_PARAM_BAD);

        // In the second run, should get cached values and not the new ones since
        // CachedFeatureFlags#cacheFieldTrialParameters() wasn't called.
        assertValuesAreFromNative();
    }

    @Test
    @EnableFeatures(
            FEATURE_A
                    + ":"
                    + STRING_PARAM_NAME
                    + "/"
                    + STRING_PARAM_TEST_OVERRIDE
                    + "/"
                    + BOOLEAN_PARAM_NAME
                    + "/"
                    + BOOLEAN_PARAM_TEST_OVERRIDE
                    + "/"
                    + INT_PARAM_NAME
                    + "/"
                    + INT_PARAM_TEST_OVERRIDE
                    + "/"
                    + DOUBLE_PARAM_NAME
                    + "/"
                    + DOUBLE_PARAM_TEST_OVERRIDE
                    + "/"
                    + STRING2_PARAM_NAME
                    + "/"
                    + STRING2_PARAM_TEST_OVERRIDE)
    public void testAnnotationOverride() {
        // Should not take priority over the overrides
        CachedFlagUtils.cacheFieldTrialParameters(mParamsToCache);

        assertValuesAreOverrides();
    }

    @Test
    public void testSetForTesting() {
        mStringParam.setForTesting(STRING_PARAM_TEST_OVERRIDE);
        mBooleanParam.setForTesting(BOOLEAN_PARAM_TEST_OVERRIDE);
        mIntParam.setForTesting(INT_PARAM_TEST_OVERRIDE);
        mDoubleParam.setForTesting(DOUBLE_PARAM_TEST_OVERRIDE);
        mString2Param.setForTesting(STRING2_PARAM_TEST_OVERRIDE);

        // Should not take priority over the overrides
        CachedFlagUtils.cacheFieldTrialParameters(mParamsToCache);

        assertValuesAreOverrides();
    }

    private void assertValuesAreDefault() {
        assertEquals(STRING_PARAM_DEFAULT, mStringParam.getValue());
        assertEquals(BOOLEAN_PARAM_DEFAULT, mBooleanParam.getValue());
        assertEquals(INT_PARAM_DEFAULT, mIntParam.getValue());
        assertEquals(DOUBLE_PARAM_DEFAULT, mDoubleParam.getValue(), 1e-6f);
        assertEquals(STRING2_PARAM_DEFAULT, mString2Param.getValue());
    }

    private void assertValuesAreFromNative() {
        assertEquals(STRING_PARAM_NATIVE, mStringParam.getValue());
        assertEquals(BOOLEAN_PARAM_NATIVE, mBooleanParam.getValue());
        assertEquals(INT_PARAM_NATIVE, mIntParam.getValue());
        assertEquals(DOUBLE_PARAM_NATIVE, mDoubleParam.getValue(), 1e-6f);
        assertEquals(STRING2_PARAM_DEFAULT, mString2Param.getValue()); // Special case
    }

    private void assertValuesAreOverrides() {
        assertEquals(STRING_PARAM_TEST_OVERRIDE, mStringParam.getValue());
        assertEquals(BOOLEAN_PARAM_TEST_OVERRIDE, mBooleanParam.getValue());
        assertEquals(INT_PARAM_TEST_OVERRIDE, mIntParam.getValue());
        assertEquals(DOUBLE_PARAM_TEST_OVERRIDE, mDoubleParam.getValue(), 1e-6f);
        assertEquals(STRING2_PARAM_TEST_OVERRIDE, mString2Param.getValue());
    }
}
