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
    private final int mModifiedDay;
    private final int mModifiedMonth;
    private final int mModifiedYear;
    private final int mUseCount;

    @CalledByNative
    public EntityMetadata(int day, int month, int year, int useCount) {
        mModifiedDay = day;
        mModifiedMonth = month;
        mModifiedYear = year;
        mUseCount = useCount;
    }

    @CalledByNative
    public int getModifiedDay() {
        return mModifiedDay;
    }

    @CalledByNative
    public int getModifiedMonth() {
        return mModifiedMonth;
    }

    @CalledByNative
    public int getModifiedYear() {
        return mModifiedYear;
    }

    @CalledByNative
    public int getUseCount() {
        return mUseCount;
    }
}
