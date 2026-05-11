// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Metadata for the {@link EntityInstance}. */
@JNINamespace("autofill")
@NullMarked
public class EntityMetadata {
    // The dates are stored as raw long values to avoid using java.time.*.
    private final long mModifiedTime;
    private final int mUseCount;

    @CalledByNative
    public EntityMetadata(long modifiedTimeMillis, int useCount) {
        mModifiedTime = modifiedTimeMillis;
        mUseCount = useCount;
    }

    @CalledByNative
    public long getModifiedTimeMillis() {
        return mModifiedTime;
    }

    @CalledByNative
    public int getUseCount() {
        return mUseCount;
    }
}
