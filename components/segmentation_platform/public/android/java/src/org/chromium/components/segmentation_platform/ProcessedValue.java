// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.url.GURL;

/**
 * Represents a single value to be used as an input to a segmentation model. Its native equivalent
 * is found here: components/segmentation_platform/public/types/processed_value.h.
 */
public class ProcessedValue {
    static final String TAG = "ProcessedValue";

    @ProcessedValueType public int type = ProcessedValueType.UNKNOWN;
    public boolean booleanValue;
    public int intValue;
    public float floatValue;
    public double doubleValue;
    public String stringValue;
    public long timeValue;
    public long int64Value;
    public GURL urlValue;

    private ProcessedValue() {}

    public static ProcessedValue fromBoolean(boolean booleanValue) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.BOOL;
        processedValue.booleanValue = booleanValue;
        return processedValue;
    }

    public static ProcessedValue fromInt(int intValue) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.INT;
        processedValue.intValue = intValue;
        return processedValue;
    }

    public static ProcessedValue fromFloat(float floatValue) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.FLOAT;
        processedValue.floatValue = floatValue;
        return processedValue;
    }

    public static ProcessedValue fromDouble(double doubleValue) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.DOUBLE;
        processedValue.doubleValue = doubleValue;
        return processedValue;
    }

    public static ProcessedValue fromString(@NonNull String stringValue) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.STRING;
        if (stringValue == null) {
            Log.w(TAG, "Null strings aren't supported. Replacing with empty string");
            processedValue.stringValue = "";
        } else {
            processedValue.stringValue = stringValue;
        }
        return processedValue;
    }

    /**
     * Creates a new ProcessedValue storing a time. When moved to native it'll be converted into a
     * base::Time value.
     * @param timeMillis A time value coming from {@code System.currentTimeMillis()}, when moved to
     *         native it'll be passed to base::Time::FromMillisecondsSinceUnixEpoch.
     * @return a ProcessedValue instance of type TIME.
     */
    public static ProcessedValue fromTimeMillis(long timeMillis) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.TIME;
        processedValue.timeValue = timeMillis;
        return processedValue;
    }

    public static ProcessedValue fromInt64(long int64Value) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.INT64;
        processedValue.int64Value = int64Value;
        return processedValue;
    }

    public static ProcessedValue fromGURL(@NonNull GURL urlValue) {
        ProcessedValue processedValue = new ProcessedValue();
        processedValue.type = ProcessedValueType.URL;
        if (urlValue == null) {
            Log.w(TAG, "Null GURL aren't supported. Replacing with empty GURL.");
            processedValue.urlValue = GURL.emptyGURL();
        } else {
            processedValue.urlValue = urlValue;
        }
        return processedValue;
    }
}
