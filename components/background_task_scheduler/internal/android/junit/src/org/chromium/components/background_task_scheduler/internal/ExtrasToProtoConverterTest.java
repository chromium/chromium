// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.List;

/** Unit tests for {@link ExtrasToProtoConverter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExtrasToProtoConverterTest {
    private static final String BOOLEAN_KEY = "B1";
    private static final boolean BOOLEAN_VALUE = true;
    private static final String BOOLEAN_ARRAY_KEY = "B2";
    private static final boolean[] BOOLEAN_ARRAY_VALUE = {false, true, false};

    private static final String DOUBLE_KEY = "D1";
    private static final double DOUBLE_VALUE = 43.2;
    private static final String DOUBLE_ARRAY_KEY = "D2";
    private static final double[] DOUBLE_ARRAY_VALUE = {1.2, 5.678};

    private static final String INT_KEY = "I1";
    private static final int INT_VALUE = 10;
    private static final String INT_ARRAY_KEY = "I2";
    private static final int[] INT_ARRAY_VALUE = {3, 87, 987};

    private static final String LONG_KEY = "L1";
    private static final long LONG_VALUE = 47244424241L;
    private static final String LONG_ARRAY_KEY = "L2";
    private static final long[] LONG_ARRAY_VALUE = {8744423435L, 748244111123L};

    private static final String STRING_KEY = "S1";
    private static final String STRING_VALUE = "Extras";
    private static final String STRING_ARRAY_KEY = "S2";
    private static final String[] STRING_ARRAY_VALUE = {"ExtrasArray"};

    private static final String NULL_ARRAY_KEY = "N1";
    private static final String NULL_STRING_KEY = "N2";

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testBundleToProtoAndBack() {
        // Construct Bundle object to store extras.
        Bundle extras = new Bundle();

        extras.putBoolean(BOOLEAN_KEY, BOOLEAN_VALUE);
        extras.putBooleanArray(BOOLEAN_ARRAY_KEY, BOOLEAN_ARRAY_VALUE);

        extras.putDouble(DOUBLE_KEY, DOUBLE_VALUE);
        extras.putDoubleArray(DOUBLE_ARRAY_KEY, DOUBLE_ARRAY_VALUE);

        extras.putInt(INT_KEY, INT_VALUE);
        extras.putIntArray(INT_ARRAY_KEY, INT_ARRAY_VALUE);

        extras.putLong(LONG_KEY, LONG_VALUE);
        extras.putLongArray(LONG_ARRAY_KEY, LONG_ARRAY_VALUE);

        extras.putString(STRING_KEY, STRING_VALUE);
        extras.putStringArray(STRING_ARRAY_KEY, STRING_ARRAY_VALUE);

        extras.putIntArray(NULL_ARRAY_KEY, null);
        extras.putString(NULL_STRING_KEY, null);

        // Convert extras to the proto representation.
        List<ScheduledTaskProto.ScheduledTask.ExtraItem> protoExtras =
                ExtrasToProtoConverter.convertExtrasToProtoExtras(extras);

        // Check conversion.
        checkExtrasToProtoConversion(protoExtras);

        // Convert proto representation to extras.
        Bundle convertedExtras = ExtrasToProtoConverter.convertProtoExtrasToExtras(protoExtras);

        // Check conversion.
        checkProtoToExtrasConversion(extras, convertedExtras);
    }

    static void checkExtrasToProtoConversion(
            List<ScheduledTaskProto.ScheduledTask.ExtraItem> protoExtras) {
        assertEquals(12, protoExtras.size());
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(BOOLEAN_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setBooleanValue(BOOLEAN_VALUE))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(BOOLEAN_ARRAY_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.BOOLEAN_ARRAY)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setBooleanValue(BOOLEAN_ARRAY_VALUE[0]))
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setBooleanValue(BOOLEAN_ARRAY_VALUE[1]))
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setBooleanValue(BOOLEAN_ARRAY_VALUE[2]))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(DOUBLE_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setDoubleValue(DOUBLE_VALUE))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(DOUBLE_ARRAY_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.DOUBLE_ARRAY)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setDoubleValue(DOUBLE_ARRAY_VALUE[0]))
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setDoubleValue(DOUBLE_ARRAY_VALUE[1]))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(INT_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setIntValue(INT_VALUE))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(INT_ARRAY_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.INT_ARRAY)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setIntValue(INT_ARRAY_VALUE[0]))
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setIntValue(INT_ARRAY_VALUE[1]))
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setIntValue(INT_ARRAY_VALUE[2]))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(LONG_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setLongValue(LONG_VALUE))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(LONG_ARRAY_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.LONG_ARRAY)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setLongValue(LONG_ARRAY_VALUE[0]))
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setLongValue(LONG_ARRAY_VALUE[1]))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(STRING_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setStringValue(STRING_VALUE))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(STRING_ARRAY_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.STRING_ARRAY)
                        .addValues(
                                ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                        .setStringValue(STRING_ARRAY_VALUE[0]))
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(NULL_ARRAY_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.NULL)
                        .build()));
        assertTrue(protoExtras.contains(
                ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                        .setKey(NULL_STRING_KEY)
                        .setType(ScheduledTaskProto.ScheduledTask.ExtraItem.Type.NULL)
                        .build()));
    }

    static void checkProtoToExtrasConversion(Bundle expectedExtras, Bundle actualExtras) {
        assertEquals(expectedExtras.keySet().size(), actualExtras.keySet().size());
        assertEquals(expectedExtras.getBoolean(BOOLEAN_KEY), actualExtras.getBoolean(BOOLEAN_KEY));
        boolean[] expectedB = expectedExtras.getBooleanArray(BOOLEAN_ARRAY_KEY);
        boolean[] actualB = actualExtras.getBooleanArray(BOOLEAN_ARRAY_KEY);
        assertEquals(expectedB.length, actualB.length);
        assertEquals(expectedB[0], actualB[0]);
        assertEquals(expectedB[1], actualB[1]);
        assertEquals(expectedB[2], actualB[2]);
        double delta = 0.0001;
        assertEquals(
                expectedExtras.getDouble(DOUBLE_KEY), actualExtras.getDouble(DOUBLE_KEY), delta);
        double[] expectedD = expectedExtras.getDoubleArray(DOUBLE_ARRAY_KEY);
        double[] actualD = actualExtras.getDoubleArray(DOUBLE_ARRAY_KEY);
        assertEquals(expectedD.length, actualD.length);
        assertEquals(expectedD[0], actualD[0], delta);
        assertEquals(expectedD[1], actualD[1], delta);
        assertEquals(expectedExtras.getInt(INT_KEY), actualExtras.getInt(INT_KEY));
        int[] expectedI = expectedExtras.getIntArray(INT_ARRAY_KEY);
        int[] actualI = actualExtras.getIntArray(INT_ARRAY_KEY);
        assertEquals(expectedI.length, actualI.length);
        assertEquals(expectedI[0], actualI[0]);
        assertEquals(expectedI[1], actualI[1]);
        assertEquals(expectedI[2], actualI[2]);
        assertEquals(expectedExtras.getLong(LONG_KEY), actualExtras.getLong(LONG_KEY));
        long[] expectedL = expectedExtras.getLongArray(LONG_ARRAY_KEY);
        long[] actualL = actualExtras.getLongArray(LONG_ARRAY_KEY);
        assertEquals(expectedL.length, actualL.length);
        assertEquals(expectedL[0], actualL[0]);
        assertEquals(expectedL[1], actualL[1]);
        assertEquals(expectedExtras.getString(STRING_KEY), actualExtras.getString(STRING_KEY));
        String[] expectedS = expectedExtras.getStringArray(STRING_ARRAY_KEY);
        String[] actualS = actualExtras.getStringArray(STRING_ARRAY_KEY);
        assertEquals(expectedS.length, actualS.length);
        assertEquals(expectedS[0], actualS[0]);
        assertEquals(
                expectedExtras.getIntArray(NULL_ARRAY_KEY), actualExtras.getString(NULL_ARRAY_KEY));
        assertEquals(
                expectedExtras.getString(NULL_STRING_KEY), actualExtras.getString(NULL_STRING_KEY));
    }
}
