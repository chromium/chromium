// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.simple_factory_key;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** An interface that provides access to a native SimpleFactoryKey. */
@JNINamespace("simple_factory_key")
public interface SimpleFactoryKeyHandle {
    /** @return A pointer to the native SimpleFactoryKey that this object wraps. */
    @CalledByNative
    long getNativeSimpleFactoryKeyPointer();
}
