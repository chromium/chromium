// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tabs;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/**
 * An opaque wrapper around the pointer to a C++ TabStripCollection. Used for safe type forwarding
 * of a C++ pointer through JNI to another C++ file.
 */
@NullMarked
public class TabStripCollection {
    /**
     * This pointer is not owned by this class and ceases to be valid if the associated Java C++
     * {@code TabCollectionTabModelImpl} is destroyed.
     */
    private final long mNativeTabStripCollection;

    @CalledByNative
    private TabStripCollection(long nativeTabStripCollection) {
        mNativeTabStripCollection = nativeTabStripCollection;
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativeTabStripCollection;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof TabStripCollection that)) return false;
        return mNativeTabStripCollection == that.mNativeTabStripCollection;
    }

    @Override
    public int hashCode() {
        return Long.hashCode(mNativeTabStripCollection);
    }
}
