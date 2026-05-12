// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Metadata for the {@link EntityInstance}. */
@JNINamespace("autofill")
@NullMarked
public class EntityMetadata {
    private final String mGuid;
    // The dates are stored as raw long values to avoid using java.time.*.
    private final long mModifiedTime;
    private final long mUseCount;
    private final long mUseDateMillis;

    @CalledByNative
    public EntityMetadata(
            @JniType("std::string") String guid,
            long modifiedTimeMillis,
            long useCount,
            long useDateMillis) {
        mGuid = guid;
        mModifiedTime = modifiedTimeMillis;
        mUseCount = useCount;
        mUseDateMillis = useDateMillis;
    }

    @CalledByNative
    public @JniType("std::string") String getGuid() {
        return mGuid;
    }

    @CalledByNative
    public long getModifiedTimeMillis() {
        return mModifiedTime;
    }

    @CalledByNative
    public long getUseCount() {
        return mUseCount;
    }

    @CalledByNative
    public long getUseDateMillis() {
        return mUseDateMillis;
    }
}
