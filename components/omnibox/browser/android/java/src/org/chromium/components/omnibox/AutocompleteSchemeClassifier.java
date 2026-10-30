// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;

import org.chromium.build.annotations.NullMarked;

/** Dumb wrapper around the pointer to the C++ class AutocompleteSchemeClassifier. */
@NullMarked
public class AutocompleteSchemeClassifier {
    @JniType("::AutocompleteSchemeClassifier")
    public interface NativeAutocompleteSchemeClassifier extends JniTypeToken {}

    private final JniUniquePtr<NativeAutocompleteSchemeClassifier> mNativePtr;

    protected AutocompleteSchemeClassifier(
            JniUniquePtr<NativeAutocompleteSchemeClassifier> nativePtr) {
        this.mNativePtr = nativePtr;
    }

    public void destroy() {
        mNativePtr.destroy();
    }

    @CalledByNative
    protected JniPtr<NativeAutocompleteSchemeClassifier> getNativePtr() {
        return mNativePtr;
    }
}
