// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map.Entry;

/**
 * Java version of InputContext, can be passed directly to native to execute segmentation models.
 */
@JNINamespace("segmentation_platform")
public class InputContext {
    private final HashMap<String, ProcessedValue> mMetadata = new HashMap<>();

    /**
     * Adds a new key-value pair.
     * @param key A string to use as metadata key.
     * @param value An instance of ProcessedValue containing the value.
     * @return This InputContext for chaining.
     */
    public InputContext addEntry(String key, ProcessedValue value) {
        mMetadata.put(key, value);
        return this;
    }

    // END OF PUBLIC API.

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void fillNativeInputContext(long target) {
        int booleanCount = 0;
        int intCount = 0;
        int floatCount = 0;
        int doubleCount = 0;
        int stringCount = 0;
        int timeCount = 0;
        int int64Count = 0;
        int urlCount = 0;

        for (Entry<String, ProcessedValue> metadataEntry : mMetadata.entrySet()) {
            switch (metadataEntry.getValue().type) {
                case ProcessedValueType.BOOL:
                    booleanCount++;
                    break;
                case ProcessedValueType.INT:
                    intCount++;
                    break;
                case ProcessedValueType.FLOAT:
                    floatCount++;
                    break;
                case ProcessedValueType.DOUBLE:
                    doubleCount++;
                    break;
                case ProcessedValueType.STRING:
                    stringCount++;
                    break;
                case ProcessedValueType.TIME:
                    timeCount++;
                    break;
                case ProcessedValueType.INT64:
                    int64Count++;
                    break;
                case ProcessedValueType.URL:
                    urlCount++;
                    break;
                default:
                    throw new IllegalArgumentException("Metadata value type not supported");
            }
        }

        String[] booleanKeys = new String[booleanCount];
        boolean[] booleanValues = new boolean[booleanCount];

        String[] intKeys = new String[intCount];
        int[] intValues = new int[intCount];

        String[] floatKeys = new String[floatCount];
        float[] floatValues = new float[floatCount];

        String[] doubleKeys = new String[doubleCount];
        double[] doubleValues = new double[doubleCount];

        String[] stringKeys = new String[stringCount];
        String[] stringValues = new String[stringCount];

        String[] timeKeys = new String[timeCount];
        long[] timeValues = new long[timeCount];

        String[] int64Keys = new String[int64Count];
        long[] int64Values = new long[int64Count];

        String[] urlKeys = new String[urlCount];
        GURL[] urlValues = new GURL[urlCount];

        int booleanIndex = 0;
        int intIndex = 0;
        int floatIndex = 0;
        int doubleIndex = 0;
        int stringIndex = 0;
        int timeIndex = 0;
        int int64Index = 0;
        int urlIndex = 0;

        for (Entry<String, ProcessedValue> metadataEntry : mMetadata.entrySet()) {
            String metadataKey = metadataEntry.getKey();
            ProcessedValue metadataValue = metadataEntry.getValue();
            switch (metadataValue.type) {
                case ProcessedValueType.BOOL:
                    booleanKeys[booleanIndex] = metadataKey;
                    booleanValues[booleanIndex] = metadataValue.booleanValue;
                    booleanIndex++;
                    break;
                case ProcessedValueType.INT:
                    intKeys[intIndex] = metadataKey;
                    intValues[intIndex] = metadataValue.intValue;
                    intIndex++;
                    break;
                case ProcessedValueType.DOUBLE:
                    doubleKeys[doubleIndex] = metadataKey;
                    doubleValues[doubleIndex] = metadataValue.doubleValue;
                    doubleIndex++;
                    break;
                case ProcessedValueType.FLOAT:
                    floatKeys[floatIndex] = metadataKey;
                    floatValues[floatIndex] = metadataValue.floatValue;
                    floatIndex++;
                    break;
                case ProcessedValueType.STRING:
                    stringKeys[stringIndex] = metadataKey;
                    stringValues[stringIndex] = metadataValue.stringValue;
                    stringIndex++;
                    break;
                case ProcessedValueType.TIME:
                    timeKeys[timeIndex] = metadataKey;
                    timeValues[timeIndex] = metadataValue.timeValue;
                    timeIndex++;
                    break;
                case ProcessedValueType.INT64:
                    int64Keys[int64Index] = metadataKey;
                    int64Values[int64Index] = metadataValue.int64Value;
                    int64Index++;
                    break;
                case ProcessedValueType.URL:
                    urlKeys[urlIndex] = metadataKey;
                    urlValues[urlIndex] = metadataValue.urlValue;
                    urlIndex++;
                    break;
                default:
                    throw new IllegalArgumentException("Type not supported");
            }
        }

        InputContextJni.get()
                .fillNative(
                        target,
                        booleanKeys,
                        booleanValues,
                        intKeys,
                        intValues,
                        floatKeys,
                        floatValues,
                        doubleKeys,
                        doubleValues,
                        stringKeys,
                        stringValues,
                        timeKeys,
                        timeValues,
                        int64Keys,
                        int64Values,
                        urlKeys,
                        urlValues);
    }

    public ProcessedValue getEntryForTesting(String key) {
        return mMetadata.get(key);
    }

    public int getSizeForTesting() {
        return mMetadata.size();
    }

    @NativeMethods
    interface Natives {
        void fillNative(
                long target,
                String[] booleanKeys,
                boolean[] booleanValues,
                String[] integerKeys,
                int[] integerValues,
                String[] floatKeys,
                float[] floatValues,
                String[] doubleKeys,
                double[] doubleValues,
                String[] stringKeys,
                String[] stringValues,
                String[] timeKeys,
                long[] timeValues,
                String[] int64Keys,
                long[] int64Values,
                String[] urlKeys,
                GURL[] urlValues);
    }
}
