// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Base64;
import android.util.Pair;

import androidx.annotation.Nullable;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.tab_group_sync.proto.TabGroupIdMetadata.TabGroupIDMetadataProto;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;

/** Persistent Storage */
public class TabGroupMetadataPersistentStore {
    private static final String TAG = "TabGroupStore";
    static final String TAB_GROUP_METADATA_PERSISTENT_STORE_FILE_NAME =
            "tab_group_metadata_persistent_store";

    /**
     * Stores a single mapping from a sync GUID to a proto.
     *
     * @param syncGuid the sync GUID for the item to store.
     * @param proto the proto to store.
     */
    public void write(String syncGuid, TabGroupIDMetadataProto proto) {
        try {
            String base64Encoded = serializeProtoToBase64EncodedString(syncGuid, proto);
            if (base64Encoded == null) {
                // Failed to base64 encode the proto. Not storing.
                return;
            }
            getSharedPreferences().edit().putString(syncGuid, base64Encoded).apply();
        } catch (RuntimeException e) {
            Log.e(TAG, "Unable to store data for sync GUID=", syncGuid);
        }
    }

    @CalledByNativeForTesting
    private static void storeDataForTesting(String syncGuid, String serializedToken) {
        TabGroupMetadataPersistentStore store = new TabGroupMetadataPersistentStore();
        TabGroupIDMetadataProto proto =
                TabGroupIDMetadataProto.newBuilder().setSerializedToken(serializedToken).build();
        store.write(syncGuid, proto);
    }

    /**
     * Deletes a persisted entry.
     *
     * @param syncGuid the GUID of the entry to delete.
     */
    public void delete(String syncGuid) {
        getSharedPreferences().edit().remove(syncGuid).apply();
    }

    /**
     * Reads all persisted data from disk.
     *
     * <p>Skips entries for anything that failed to read or parse.
     *
     * @return a Map of each of the sync GUIDs to their respective proto.
     */
    public Map<String, TabGroupIDMetadataProto> readAll() {
        try {
            Map<String, TabGroupIDMetadataProto> result =
                    new HashMap<String, TabGroupIDMetadataProto>();

            Map<String, ?> storedValues = getSharedPreferences().getAll();
            for (Map.Entry<String, ?> entry : storedValues.entrySet()) {
                String syncGuid = entry.getKey();
                Object rawValue = entry.getValue();
                if (!(rawValue instanceof String)) {
                    // `value` is null or not a String, skip entry.
                    continue;
                }
                String base64EncodedProto = (String) rawValue;
                @Nullable
                TabGroupIDMetadataProto proto =
                        parseProtoFromBase64EncodedString(syncGuid, base64EncodedProto);
                if (proto == null) {
                    // Failed to parse proto from base64 encoded value, skip entry.
                    continue;
                }
                result.put(syncGuid, proto);
            }
            return result;
        } catch (RuntimeException e) {
            Log.e(TAG, "Unable to get all TabGroupIDMetadataProtos");
            return new HashMap<String, TabGroupIDMetadataProto>();
        }
    }

    @CalledByNative
    private static Pair<String, String>[] readAllDataForMigration() {
        TabGroupMetadataPersistentStore store = new TabGroupMetadataPersistentStore();
        Map<String, TabGroupIDMetadataProto> protoMap = store.readAll();
        Pair<String, String>[] idPairs = new Pair[protoMap.size()];

        int i = 0;
        for (Map.Entry<String, TabGroupIDMetadataProto> entry : protoMap.entrySet()) {
            String syncId = entry.getKey();
            String serializedToken = entry.getValue().getSerializedToken();
            idPairs[i++] = new Pair<>(syncId, serializedToken);
        }

        return idPairs;
    }

    @CalledByNative
    private static String getFirstFromPair(Pair<String, String> pair) {
        return pair.first;
    }

    @CalledByNative
    private static String getSecondFromPair(Pair<String, String> pair) {
        return pair.second;
    }

    /** Deletes all stored content. */
    @CalledByNative
    static void clearAllData() {
        getSharedPreferences().edit().clear().apply();
    }

    private static @Nullable String serializeProtoToBase64EncodedString(
            String syncGuid, TabGroupIDMetadataProto proto) {
        try {
            ByteBuffer bb = proto.toByteString().asReadOnlyByteBuffer();
            bb.rewind();
            byte[] bytes = new byte[bb.remaining()];
            bb.get(bytes);
            return Base64.encodeToString(
                    bytes, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
        } catch (RuntimeException e) {
            Log.e(TAG, "Unable to store data for sync GUID=", syncGuid);
            return null;
        }
    }

    private static @Nullable TabGroupIDMetadataProto parseProtoFromBase64EncodedString(
            String syncGuid, String base64Encoded) {
        byte[] bytes;
        try {
            bytes =
                    Base64.decode(
                            base64Encoded, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
        } catch (RuntimeException e) {
            Log.e(TAG, "Unable to decode base64 data=", syncGuid);
            return null;
        }

        try {
            return TabGroupIDMetadataProto.parseFrom(bytes);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Unable to decode proto for sync GUID=", syncGuid);
            return null;
        }
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(
                        TAB_GROUP_METADATA_PERSISTENT_STORE_FILE_NAME, Context.MODE_PRIVATE);
    }
}
