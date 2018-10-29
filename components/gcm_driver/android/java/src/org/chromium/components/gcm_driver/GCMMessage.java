// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.Nullable;

/**
 * Represents the contents of a GCM Message that is to be handled by the GCM Driver. Can be created
 * based on data received from GCM, or serialized and deserialized to and from a Bundle.
 */
public class GCMMessage {
    @VisibleForTesting
    static final String VERSION = "v1";
    private static final String TAG = "GCMMessage";
    private static final String SERIALIZATION_CHARSET = "ISO-8859-1";
    /**
     * Keys used to store information for serialization purposes.
     */
    private static final String KEY_VERSION = "version";
    private static final String KEY_APP_ID = "appId";
    private static final String KEY_COLLAPSE_KEY = "collapseKey";
    private static final String KEY_DATA = "data";
    private static final String KEY_RAW_DATA = "rawData";
    private static final String KEY_SENDER_ID = "senderId";

    private final String mSenderId;
    private final String mAppId;

    @Nullable
    private final String mCollapseKey;
    @Nullable
    private final byte[] mRawData;

    /**
     * Array that contains pairs of entries in the format of {key, value}.
     */
    private final String[] mDataKeysAndValuesArray;

    /**
     * Creates a GCMMessage object based on data received from GCM. The extras will be filtered.
     */
    public GCMMessage(String senderId, Bundle extras) {
        String bundleCollapseKey = "collapse_key";
        String bundleGcmplex = "com.google.ipc.invalidation.gcmmplex.";
        String bundleRawData = "rawData";
        String bundleSenderId = "from";
        String bundleSubtype = "subtype";

        if (!extras.containsKey(bundleSubtype)) {
            throw new IllegalArgumentException("Received push message with no subtype");
        }

        mSenderId = senderId;
        mAppId = extras.getString(bundleSubtype);

        mCollapseKey = extras.getString(bundleCollapseKey); // May be null.
        mRawData = extras.getByteArray(bundleRawData); // May be null.

        List<String> dataKeysAndValues = new ArrayList<String>();
        for (String key : extras.keySet()) {
            if (key.equals(bundleSubtype) || key.equals(bundleSenderId)
                    || key.equals(bundleCollapseKey) || key.equals(bundleRawData)
                    || key.startsWith(bundleGcmplex)) {
                continue;
            }

            Object value = extras.get(key);
            if (!(value instanceof String)) {
                continue;
            }

            dataKeysAndValues.add(key);
            dataKeysAndValues.add((String) value);
        }

        mDataKeysAndValuesArray = dataKeysAndValues.toArray(new String[dataKeysAndValues.size()]);
    }

    /**
     * Creates a GCMMessage object based on the given bundle. Assumes that the bundle has previously
     * been created through {@link #toBundle}.
     */
    @Nullable
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static GCMMessage createFromBundle(Bundle bundle) {
        return create(bundle, new BundleReader());
    }

    /**
     * Creates a GCMMessage object based on the given JSONObject. Assumes that the JSONObject has
     * previously been created through {@link #toJSON}.
     */
    @Nullable
    public static GCMMessage createFromJSON(JSONObject messageJSON) {
        return create(messageJSON, new JSONReader());
    }

    @Nullable
    private static <T> GCMMessage create(T in, Reader<T> reader) {
        // validate() checks that the fields are present, which is different to the checks below
        // that check if required fields are not null.
        if (!validate(in, reader)) {
            return null;
        }
        if (reader.readString(in, KEY_APP_ID) == null
                || reader.readString(in, KEY_SENDER_ID) == null) {
            return null;
        }
        return new GCMMessage(in, reader);
    }

    /**
     * Validates that all required fields have been set in the given object.
     */
    private static <T> boolean validate(T in, Reader<T> reader) {
        return reader.hasKey(in, KEY_APP_ID) && reader.hasKey(in, KEY_COLLAPSE_KEY)
                && reader.hasKey(in, KEY_DATA) && reader.hasKey(in, KEY_RAW_DATA)
                && reader.hasKey(in, KEY_SENDER_ID);
    }

    private <T> GCMMessage(T source, Reader<T> reader) {
        mSenderId = reader.readString(source, KEY_SENDER_ID);
        mAppId = reader.readString(source, KEY_APP_ID);
        mCollapseKey = reader.readString(source, KEY_COLLAPSE_KEY);
        // The rawData field needs to distinguish between {not set, set but empty, set with data}.
        String rawDataString = reader.readString(source, KEY_RAW_DATA);
        if (rawDataString != null) {
            if (rawDataString.length() > 0) {
                mRawData = rawDataString.getBytes(Charset.forName(SERIALIZATION_CHARSET));
            } else {
                mRawData = new byte[0];
            }
        } else {
            mRawData = null;
        }

        mDataKeysAndValuesArray = reader.readStringArray(source, KEY_DATA);
    }

    public String getSenderId() {
        return mSenderId;
    }

    public String getAppId() {
        return mAppId;
    }

    @Nullable
    public String getCollapseKey() {
        return mCollapseKey;
    }

    /**
     * Callers are expected to not modify values in the returned byte array.
     */
    @Nullable
    public byte[] getRawData() {
        return mRawData;
    }

    /**
     * Callers are expected to not modify values in the returned byte array.
     */
    public String[] getDataKeysAndValuesArray() {
        return mDataKeysAndValuesArray;
    }

    /**
     * Returns the collapse key of GCMMessage encoded as a JSONObject. Returns null if non exists.
     * Assumes that the JSONObject has previously been created through
     * {@link #toJSON}.
     * @param jsonObject The JSONObject encoding the GCMMessage
     * @return The collapse key. Null if non-exists.
     */
    public static String peekCollapseKey(JSONObject jsonObject) {
        return jsonObject.optString(KEY_COLLAPSE_KEY, null);
    }

    /**
     * Returns the sender id of GCMMessage encoded as a JSONObject. Returns null if non exists.
     * Assumes that the JSONObject has previously been created through
     * {@link #toJSON}.
     * @param jsonObject The JSONObject encoding the GCMMessage
     * @return The collapse key. Null if non-exists.
     */
    public static String peekSenderId(JSONObject jsonObject) {
        return jsonObject.optString(KEY_SENDER_ID, null);
    }

    /**
     * Serializes the contents of this GCM Message to a new bundle that can be stored, for example
     * for purposes of scheduling a job. Only methods available in BaseBundle may be used here,
     * as it may have to be converted to a PersistableBundle.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public Bundle toBundle() {
        return serialize(new BundleWriter());
    }

    /**
     * Serializes the contents of this GCM Message to a JSONObject such that it
     * could be stored as a String.
     */
    public JSONObject toJSON() {
        return serialize(new JSONWriter());
    }

    private <T> T serialize(Writer<T> writer) {
        T out = writer.createOutputObject();
        writer.writeString(out, KEY_VERSION, VERSION);
        writer.writeString(out, KEY_SENDER_ID, mSenderId);
        writer.writeString(out, KEY_APP_ID, mAppId);
        writer.writeString(out, KEY_COLLAPSE_KEY, mCollapseKey);

        // The rawData field needs to distinguish between {not set, set but empty, set with data}.
        if (mRawData != null) {
            if (mRawData.length > 0) {
                writer.writeString(out, KEY_RAW_DATA,
                        new String(mRawData, Charset.forName(SERIALIZATION_CHARSET)));
            } else {
                writer.writeString(out, KEY_RAW_DATA, "");
            }
        } else {
            writer.writeString(out, KEY_RAW_DATA, null);
        }

        writer.writeStringArray(out, KEY_DATA, mDataKeysAndValuesArray);
        return out;
    }

    private interface Reader<T> {
        public boolean hasKey(T in, String key);
        public String readString(T in, String key);
        @Nullable
        public String[] readStringArray(T in, String key);
    }

    private static class BundleReader implements Reader<Bundle> {
        @Override
        public boolean hasKey(Bundle bundle, String key) {
            return bundle.containsKey(key);
        }

        @Override
        public String readString(Bundle bundle, String key) {
            return bundle.getString(key);
        }

        @Override
        public String[] readStringArray(Bundle bundle, String key) {
            return bundle.getStringArray(key);
        }
    }

    private static class JSONReader implements Reader<JSONObject> {
        @Override
        public boolean hasKey(JSONObject jsonObj, String key) {
            return jsonObj.has(key);
        }

        @Override
        public String readString(JSONObject jsonObj, String key) {
            if (JSONObject.NULL.equals(jsonObj.opt(key))) {
                return null;
            }
            return jsonObj.optString(key, /*fallback=*/null);
        }

        @Override
        public String[] readStringArray(JSONObject jsonObj, String key) {
            JSONArray jsonArray = jsonObj.optJSONArray(key);
            if (jsonArray == null) {
                return null;
            }
            List<String> strings = new ArrayList<String>(jsonArray.length());
            for (int i = 0; i < jsonArray.length(); i++) {
                strings.add(jsonArray.optString(i));
            }
            return strings.toArray(new String[strings.size()]);
        }
    }

    private interface Writer<T> {
        public T createOutputObject();
        public void writeString(T out, String key, String value);
        public void writeStringArray(T out, String key, String[] value);
    }

    private class BundleWriter implements Writer<Bundle> {
        @Override
        public Bundle createOutputObject() {
            return new Bundle();
        }

        @Override
        public void writeString(Bundle bundle, String key, String value) {
            bundle.putString(key, value);
        }

        @Override
        public void writeStringArray(Bundle bundle, String key, String[] value) {
            bundle.putStringArray(key, value);
        }
    }

    private class JSONWriter implements Writer<JSONObject> {
        @Override
        public JSONObject createOutputObject() {
            return new JSONObject();
        }

        @Override
        public void writeString(JSONObject jsonObj, String key, String value) {
            try {
                if (value == null) {
                    jsonObj.put(key, JSONObject.NULL);
                    return;
                }
                jsonObj.put(key, value);
            } catch (JSONException e) {
                Log.e(GCMMessage.TAG, "Error when serializing a GCMMessage into a JSONObject.");
            }
        }

        @Override
        public void writeStringArray(JSONObject jsonObj, String key, String[] value) {
            JSONArray jsonArray = new JSONArray();
            try {
                for (String str : value) {
                    jsonArray.put(str);
                }
                jsonObj.put(key, jsonArray);
            } catch (JSONException e) {
                Log.e(GCMMessage.TAG, "Error when serializing a GCMMessage into a JSONObject.");
            }
        }
    }
}
