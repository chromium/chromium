// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.AutofillSuggestion.Payload;

import java.util.Objects;

@JNINamespace("autofill")
@NullMarked
public final class AtMemoryPayload implements Payload {
    private final String mTypeName;

    @CalledByNative
    public AtMemoryPayload(@JniType("std::u16string") String typeName) {
        mTypeName = typeName;
    }

    public String getTypeName() {
        return mTypeName;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof AtMemoryPayload other)) {
            return false;
        }
        return this.mTypeName.equals(other.mTypeName);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTypeName);
    }
}
