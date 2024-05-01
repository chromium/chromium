// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.tab_group_sync.proto.TabGroupIdMetadata.TabGroupIDMetadataProto;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Java counterpart to the C++ TabGroupStoreDelegateAndroid class. */
@JNINamespace("tab_groups")
public class TabGroupStoreDelegate {
    private final TabGroupMetadataPersistentStore mStorage = new TabGroupMetadataPersistentStore();

    private long mNativePtr;

    @CalledByNative
    private static TabGroupStoreDelegate create(long nativePtr) {
        return new TabGroupStoreDelegate(nativePtr);
    }

    TabGroupStoreDelegate(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private void storeTabGroupIDMetadata(String syncGuid, String serializedToken) {
        TabGroupIDMetadataProto proto =
                TabGroupIDMetadataProto.newBuilder().setSerializedToken(serializedToken).build();
        mStorage.write(syncGuid, proto);
    }

    @CalledByNative
    private void deleteTabGroupIDMetadata(String syncGuid) {
        mStorage.delete(syncGuid);
    }

    @CalledByNative
    private void getTabGroupIDMetadatas(long nativeCallbackPtr) {
        if (mNativePtr == 0) {
            // Native has been deleted, so ignore this request.
            return;
        }

        List<String> syncGuidList = new ArrayList<>();
        List<String> serializedTokenList = new ArrayList<>();

        Map<String, TabGroupIDMetadataProto> persistedData = mStorage.readAll();
        for (Map.Entry<String, TabGroupIDMetadataProto> entry : persistedData.entrySet()) {
            String syncGuid = entry.getKey();
            if (syncGuid == null) {
                // The key from the storage engine is null, which is invalid. Skip this entry.
                continue;
            }
            TabGroupIDMetadataProto proto = entry.getValue();
            if (proto == null) {
                // The store is not supposed to return empty protos, but we should not propagate.
                continue;
            }
            String serializedToken = proto.getSerializedToken();

            syncGuidList.add(syncGuid);
            serializedTokenList.add(serializedToken);
        }

        TabGroupStoreDelegateJni.get()
                .onGetTabGroupIDMetadata(
                        mNativePtr,
                        nativeCallbackPtr,
                        syncGuidList.stream().toArray(String[]::new),
                        serializedTokenList.stream().toArray(String[]::new));
    }

    @NativeMethods
    interface Natives {
        void onGetTabGroupIDMetadata(
                long nativeTabGroupStoreDelegateAndroid,
                long callbackPtr,
                String[] syncGuids,
                String[] serializedTokens);
    }
}
