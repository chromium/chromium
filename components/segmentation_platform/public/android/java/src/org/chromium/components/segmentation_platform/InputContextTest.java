// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashSet;

/** Tests for InputContext. */
@RunWith(BaseRobolectricTestRunner.class)
public class InputContextTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock InputContext.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(InputContextJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void fillNativeInputContextCallsNative() {
        InputContext inputContext = new InputContext();

        inputContext.addEntry("boolean_param", ProcessedValue.fromBoolean(true));
        inputContext.addEntry("int param", ProcessedValue.fromInt(-4));
        inputContext.addEntry("float_param", ProcessedValue.fromFloat(13.37f));
        inputContext.addEntry("double_value", ProcessedValue.fromDouble(13.38d));
        inputContext.addEntry("string_value", ProcessedValue.fromString("Hello, World!"));
        inputContext.addEntry(
                "time_value", ProcessedValue.fromTimeMillis(TimeUtils.currentTimeMillis()));
        inputContext.addEntry("int64 value", ProcessedValue.fromInt64(Long.MIN_VALUE));
        inputContext.addEntry("url_value", ProcessedValue.fromGURL(JUnitTestGURLs.EXAMPLE_URL));

        // Native calls this method with a pointer to a native InputContext.
        inputContext.fillNativeInputContext(0x12345678);

        Mockito.verify(mNativeMock)
                .fillNative(
                        0x12345678,
                        new String[] {"boolean_param"},
                        new boolean[] {true},
                        new String[] {"int param"},
                        new int[] {-4},
                        new String[] {"float_param"},
                        new float[] {13.37f},
                        new String[] {"double_value"},
                        new double[] {13.38d},
                        new String[] {"string_value"},
                        new String[] {"Hello, World!"},
                        new String[] {"time_value"},
                        new long[] {TimeUtils.currentTimeMillis()},
                        new String[] {"int64 value"},
                        new long[] {Long.MIN_VALUE},
                        new String[] {"url_value"},
                        new GURL[] {JUnitTestGURLs.EXAMPLE_URL});
    }

    @Test
    public void nullValuesAreReplaced() {
        InputContext inputContext = new InputContext();
        inputContext.addEntry("null_url", ProcessedValue.fromGURL(null));
        inputContext.addEntry("null_string", ProcessedValue.fromString(null));

        inputContext.fillNativeInputContext(0x12345678);

        Mockito.verify(mNativeMock)
                .fillNative(
                        0x12345678,
                        new String[] {},
                        new boolean[] {},
                        new String[] {},
                        new int[] {},
                        new String[] {},
                        new float[] {},
                        new String[] {},
                        new double[] {},
                        new String[] {"null_string"},
                        new String[] {""},
                        new String[] {},
                        new long[] {},
                        new String[] {},
                        new long[] {},
                        new String[] {"null_url"},
                        new GURL[] {GURL.emptyGURL()});
    }

    @Test
    public void fillNativeInputContextCallsNative_keysAndValuesKeepPairing() {
        ArgumentCaptor<Object> integerArrayArgumentCaptor = ArgumentCaptor.forClass(Object.class);
        ArgumentCaptor<Object> stringArrayArgumentCaptor = ArgumentCaptor.forClass(Object.class);
        InputContext inputContext = new InputContext();

        inputContext.addEntry("int_param", ProcessedValue.fromInt(1));
        inputContext.addEntry("negative_int_param", ProcessedValue.fromInt(-4));
        inputContext.addEntry("zero_int_param", ProcessedValue.fromInt(0));
        inputContext.addEntry("large_int_param", ProcessedValue.fromInt(Integer.MAX_VALUE));

        // Native calls this method with a pointer to a native InputContext.
        inputContext.fillNativeInputContext(0x12345678);

        Mockito.verify(mNativeMock)
                .fillNative(
                        eq(0x12345678L),
                        any(),
                        any(),
                        (String[]) stringArrayArgumentCaptor.capture(),
                        (int[]) integerArrayArgumentCaptor.capture(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any());

        String[] keys = (String[]) stringArrayArgumentCaptor.getValue();
        int[] values = (int[]) integerArrayArgumentCaptor.getValue();

        // Ensure both key and value arrays are the same length.
        assertEquals(4, keys.length);
        assertEquals(4, values.length);

        HashSet<Pair<String, Integer>> keyValuePairs = new HashSet<>();

        for (int i = 0; i < keys.length; i++) {
            keyValuePairs.add(new Pair<>(keys[i], values[i]));
        }

        // Ensure the key and value pair arrays don't mix up keys and values.
        assertTrue(keyValuePairs.contains(new Pair<>("int_param", 1)));
        assertTrue(keyValuePairs.contains(new Pair<>("negative_int_param", -4)));
        assertTrue(keyValuePairs.contains(new Pair<>("zero_int_param", 0)));
        assertTrue(keyValuePairs.contains(new Pair<>("large_int_param", Integer.MAX_VALUE)));
    }
}
