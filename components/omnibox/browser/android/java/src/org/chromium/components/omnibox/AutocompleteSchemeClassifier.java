// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.chromium.base.annotations.CalledByNative;

/**
 * Dumb wrapper around the pointer to the C++ class AutocompleteSchemeClassifier.
 */
public class AutocompleteSchemeClassifier {
    private long mNativePtr;

    protected AutocompleteSchemeClassifier(long nativePtr) {
        this.mNativePtr = nativePtr;
    }

    public void destroy() {}

    @CalledByNative
    protected long getNativePtr() {
        return mNativePtr;
    }
}