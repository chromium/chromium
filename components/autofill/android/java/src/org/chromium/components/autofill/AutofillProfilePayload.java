// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.AutofillSuggestion.Payload;

import java.util.Objects;

@JNINamespace("autofill")
@NullMarked
public final class AutofillProfilePayload implements Payload {
    private final String mGuid;

    @CalledByNative
    @VisibleForTesting
    public AutofillProfilePayload(@JniType("std::string") String guid) {
        mGuid = guid;
    }

    public String getGuid() {
        return mGuid;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof AutofillProfilePayload other)) {
            return false;
        }
        return this.mGuid.equals(other.mGuid);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mGuid);
    }
}
