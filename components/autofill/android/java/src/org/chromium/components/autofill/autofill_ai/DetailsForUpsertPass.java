// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.payments.LegalMessageLine;

import java.util.List;

/** Information details required for an upsert pass request. */
@JNINamespace("autofill")
@NullMarked
public class DetailsForUpsertPass {
    private final List<LegalMessageLine> mLegalMessageLines;
    private final String mContextToken;

    @CalledByNative
    public DetailsForUpsertPass(
            @JniType("std::vector") List<LegalMessageLine> legalMessageLines,
            @JniType("std::string") String contextToken) {
        mLegalMessageLines = legalMessageLines;
        mContextToken = contextToken;
    }

    public List<LegalMessageLine> getLegalMessageLines() {
        return mLegalMessageLines;
    }

    public String getContextToken() {
        return mContextToken;
    }
}
