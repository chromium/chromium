// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** A convenience class for displaying keyed values in a dropdown. */
@JNINamespace("autofill")
@NullMarked
public class DropdownKeyValue extends Pair<String, String> {
    @CalledByNative
    public DropdownKeyValue(
            @JniType("std::string") String key, @JniType("std::u16string") String value) {
        super(key, value);
    }

    /**
     * @return The key identifier.
     */
    @CalledByNative
    public @JniType("std::string") String getKey() {
        return super.first;
    }

    /**
     * @return The human-readable localized display value.
     */
    @CalledByNative
    public @JniType("std::u16string") String getValue() {
        return super.second;
    }

    @Override
    public String toString() {
        return super.second.toString();
    }
}
