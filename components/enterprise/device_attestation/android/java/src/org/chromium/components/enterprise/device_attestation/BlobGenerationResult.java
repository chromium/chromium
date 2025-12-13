// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.device_attestation;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Object to hold attestation blob generation result. */
@NullMarked
public class BlobGenerationResult {
    // The newly generated attestation blob if one is successfully generated, empty otherwise.
    private final String mAttestationBlob;
    // The error/exception occurred if blob generation failed, used for logging.
    private final String mErrorMessage;

    public BlobGenerationResult(String attestationBlob, String errorMessage) {
        mAttestationBlob = attestationBlob;
        mErrorMessage = errorMessage;
    }

    @CalledByNative
    public String getAttestationBlob() {
        return mAttestationBlob;
    }

    @CalledByNative
    public String getErrorMessage() {
        return mErrorMessage;
    }
}
