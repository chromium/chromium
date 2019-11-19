// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.os.Bundle;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * Converts the extras from {@link Bundle} to the specific proto representation in
 * {@link ScheduledTaskProto.ScheduledTask}.
 */
public final class ExtrasToProtoConverter {
    private static final String TAG = "BTSExtrasC";

    private ExtrasToProtoConverter() {}

    /**
     * Converts information stored in a {@link Bundle} to its proto buffer representation to be
     * stored by {@link BackgroundTaskSchedulerPrefs}. In case null values are passed, they are
     * stored as null Strings.
     *
     * @param extras the extras stored for the {@link BackgroundTask}.
     * @return a list of proto messages representing each extra.
     */
    static List<ScheduledTaskProto.ScheduledTask.ExtraItem> convertExtrasToProtoExtras(
            Bundle extras) {
        List<ScheduledTaskProto.ScheduledTask.ExtraItem> protoExtras = new ArrayList<>();

        for (String key : extras.keySet()) {
            Object obj = extras.get(key);

            ScheduledTaskProto.ScheduledTask.ExtraItem.Type type;
            List<ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue> values = new ArrayList<>();

            if (obj instanceof Boolean) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE;
                values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                   .setBooleanValue((Boolean) obj)
                                   .build());
            } else if (obj instanceof boolean[]) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.BOOLEAN_ARRAY;
                boolean[] bundleValues = extras.getBooleanArray(key);
                for (int i = 0; i < bundleValues.length; i++) {
                    values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                       .setBooleanValue(bundleValues[i])
                                       .build());
                }
            } else if (obj instanceof Double) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE;
                values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                   .setDoubleValue((Double) obj)
                                   .build());
            } else if (obj instanceof double[]) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.DOUBLE_ARRAY;
                double[] bundleValues = extras.getDoubleArray(key);
                for (int i = 0; i < bundleValues.length; i++) {
                    values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                       .setDoubleValue(bundleValues[i])
                                       .build());
                }
            } else if (obj instanceof Integer) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE;
                values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                   .setIntValue((Integer) obj)
                                   .build());
            } else if (obj instanceof int[]) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.INT_ARRAY;
                int[] bundleValues = extras.getIntArray(key);
                for (int i = 0; i < bundleValues.length; i++) {
                    values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                       .setIntValue(bundleValues[i])
                                       .build());
                }
            } else if (obj instanceof Long) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE;
                values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                   .setLongValue((Long) obj)
                                   .build());
            } else if (obj instanceof long[]) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.LONG_ARRAY;
                long[] bundleValues = extras.getLongArray(key);
                for (int i = 0; i < bundleValues.length; i++) {
                    values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                       .setLongValue(bundleValues[i])
                                       .build());
                }
            } else if (obj instanceof String) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.SINGLE;
                values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                   .setStringValue((String) obj)
                                   .build());
            } else if (obj instanceof String[]) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.STRING_ARRAY;
                String[] bundleValues = extras.getStringArray(key);
                for (int i = 0; i < bundleValues.length; i++) {
                    values.add(ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue.newBuilder()
                                       .setStringValue(bundleValues[i])
                                       .build());
                }
            } else if (obj == null) {
                type = ScheduledTaskProto.ScheduledTask.ExtraItem.Type.NULL;
            } else {
                Log.e(TAG, "Value not in the list of supported extras for key " + key + ": " + obj);
                continue;
            }

            protoExtras.add(ScheduledTaskProto.ScheduledTask.ExtraItem.newBuilder()
                                    .setKey(key)
                                    .setType(type)
                                    .addAllValues(values)
                                    .build());
        }

        return protoExtras;
    }

    /**
     * Converts information stored in the proto buffer to a {@link Bundle} to be used by the
     * {@link BackgroundTask}.
     *
     * @param protoExtras list of {@link ScheduledTaskProto.ScheduledTask.ExtraItem} objects that
     * store the extras for the {@link BackgroundTask}.
     * @return a {@link Bundle} object with all the extras.
     */
    static Bundle convertProtoExtrasToExtras(
            List<ScheduledTaskProto.ScheduledTask.ExtraItem> protoExtras) {
        Bundle extras = new Bundle();

        for (ScheduledTaskProto.ScheduledTask.ExtraItem protoExtra : protoExtras) {
            List<ScheduledTaskProto.ScheduledTask.ExtraItem.ExtraValue> protoValues =
                    protoExtra.getValuesList();

            switch (protoExtra.getType()) {
                case SINGLE:
                    switch (protoValues.get(0).getOneofValueCase()) {
                        case BOOLEAN_VALUE:
                            extras.putBoolean(
                                    protoExtra.getKey(), protoValues.get(0).getBooleanValue());
                            break;
                        case DOUBLE_VALUE:
                            extras.putDouble(
                                    protoExtra.getKey(), protoValues.get(0).getDoubleValue());
                            break;
                        case INT_VALUE:
                            extras.putInt(protoExtra.getKey(), protoValues.get(0).getIntValue());
                            break;
                        case LONG_VALUE:
                            extras.putLong(protoExtra.getKey(), protoValues.get(0).getLongValue());
                            break;
                        case STRING_VALUE:
                            extras.putString(
                                    protoExtra.getKey(), protoValues.get(0).getStringValue());
                            break;
                        default:
                            Log.e(TAG,
                                    "For key " + protoExtra.getKey() + " no value was set,"
                                            + " even though the extra was not null.");
                            return null;
                    }
                    break;
                case BOOLEAN_ARRAY:
                    boolean[] booleanValues = new boolean[protoValues.size()];
                    for (int i = 0; i < protoValues.size(); i++) {
                        booleanValues[i] = protoValues.get(i).getBooleanValue();
                    }
                    extras.putBooleanArray(protoExtra.getKey(), booleanValues);
                    break;
                case DOUBLE_ARRAY:
                    double[] doubleValues = new double[protoValues.size()];
                    for (int i = 0; i < protoValues.size(); i++) {
                        doubleValues[i] = protoValues.get(i).getDoubleValue();
                    }
                    extras.putDoubleArray(protoExtra.getKey(), doubleValues);
                    break;
                case INT_ARRAY:
                    int[] intValues = new int[protoValues.size()];
                    for (int i = 0; i < protoValues.size(); i++) {
                        intValues[i] = protoValues.get(i).getIntValue();
                    }
                    extras.putIntArray(protoExtra.getKey(), intValues);
                    break;
                case LONG_ARRAY:
                    long[] longValues = new long[protoValues.size()];
                    for (int i = 0; i < protoValues.size(); i++) {
                        longValues[i] = protoValues.get(i).getLongValue();
                    }
                    extras.putLongArray(protoExtra.getKey(), longValues);
                    break;
                case STRING_ARRAY:
                    String[] stringValues = new String[protoValues.size()];
                    for (int i = 0; i < protoValues.size(); i++) {
                        stringValues[i] = protoValues.get(i).getStringValue();
                    }
                    extras.putStringArray(protoExtra.getKey(), stringValues);
                    break;
                case NULL:
                    extras.putString(protoExtra.getKey(), null);
                    break;
                default:
                    Log.e(TAG,
                            "For key " + protoExtra.getKey()
                                    + " an invalid type was found: " + protoExtra.getType());
            }
        }

        return extras;
    }
}
