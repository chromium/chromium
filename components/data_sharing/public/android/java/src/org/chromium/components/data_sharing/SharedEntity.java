// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;
import org.chromium.components.sync.protocol.EntitySpecifics;

/** Information about a shared entity. */
@JNINamespace("data_sharing")
public class SharedEntity {
    public final String groupId;
    public final String name;
    public final long version;
    // Time when the entity is updated, using unix Epoch in milliseconds.
    public final long updateTime;
    // Time when the entity is created, using unix Epoch in milliseconds.
    public final long createTime;
    public final String clientTagHash;
    public final EntitySpecifics specifics;

    private static final String TAG = "SharedEntity";

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public SharedEntity(
            String groupId,
            String name,
            long version,
            long updateTime,
            long createTime,
            String clientTagHash,
            EntitySpecifics specifics) {
        this.groupId = groupId;
        this.name = name;
        this.version = version;
        this.updateTime = updateTime;
        this.createTime = createTime;
        this.clientTagHash = clientTagHash;
        this.specifics = specifics;
    }

    @CalledByNative
    private static SharedEntity createSharedEntity(
            String groupId,
            String name,
            long version,
            long updateTime,
            long createTime,
            String clientTagHash,
            byte[] serializedSpecifics) {
        try {
            return new SharedEntity(
                    groupId,
                    name,
                    version,
                    updateTime,
                    createTime,
                    clientTagHash,
                    serializedSpecifics == null
                            ? null
                            : EntitySpecifics.parseFrom(serializedSpecifics));
        } catch (Exception e) {
            Log.e(TAG, "Failed to parse entity specifics.", e);
            return null;
        }
    }
}
