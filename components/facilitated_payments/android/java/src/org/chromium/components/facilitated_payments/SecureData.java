// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Class containing the key value pairs for the secure data returned from Payments backend. */
@JNINamespace("payments::facilitated")
@NullMarked
public class SecureData {
    private final int mKey;
    private final String mValue;

    @CalledByNative
    public SecureData(int key, String value) {
        this.mKey = key;
        this.mValue = value;
    }

    /** Returns the key for the SecureData. */
    public int getKey() {
        return mKey;
    }

    /** Returns the value for the SecureData. */
    public String getValue() {
        return mValue;
    }
}
