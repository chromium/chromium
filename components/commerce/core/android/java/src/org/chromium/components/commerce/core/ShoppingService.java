// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** A central hub for accessing shopping and product infomration. */
@JNINamespace("commerce")
public class ShoppingService {
    /** A pointer to the native side of the object. */
    private long mNativeShoppingService;

    /** Private constructor to ensure construction only happens by native. */
    private ShoppingService(long nativePtr) {
        mNativeShoppingService = nativePtr;
    }

    @CalledByNative
    private void destroy() {
        mNativeShoppingService = 0;
    }

    @CalledByNative
    private static ShoppingService create(long nativePtr) {
        return new ShoppingService(nativePtr);
    }

    @NativeMethods
    interface Natives {}
}
