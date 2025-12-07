// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Dumb wrapper around the pointer to the C++ class AutocompleteSchemeClassifier. */
@NullMarked
public class AutocompleteSchemeClassifier {
    private final long mNativePtr;

    protected AutocompleteSchemeClassifier(long nativePtr) {
        this.mNativePtr = nativePtr;
    }

    public void destroy() {}

    @CalledByNative
    protected long getNativePtr() {
        return mNativePtr;
    }
}
