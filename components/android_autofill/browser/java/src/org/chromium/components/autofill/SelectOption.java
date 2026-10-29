// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Java counterpart of native {@code autofill::SelectOption}. */
@JNINamespace("autofill")
@NullMarked
public class SelectOption {
    private final String mValue;
    private final String mText;

    @CalledByNative
    public SelectOption(
            @JniType("std::u16string") String value, @JniType("std::u16string") String text) {
        mValue = value;
        mText = text;
    }

    /** Returns the option's {@code value} attribute, or its text content if absent. */
    public String getValue() {
        return mValue;
    }

    /** Returns the option's {@code label} attribute, or its text content if absent. */
    public String getText() {
        return mText;
    }
}
