// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.device_attestation;

import org.chromium.build.annotations.NullMarked;

/** Delegate for attestation blob generation functions implemented downstream for Google Chrome. */
@NullMarked
public interface AttestationBlobGeneratorDelegate {
    // TODO(crbug.com/448416033): Remove this method once we finish adding flow name in internal
    // implementation.
    /**
     * Generates the desired attestation blob with provided fields for content binding
     *
     * @param requestHash The hash of the request without attestation payload plus nonce.
     * @param timestampHash The hash of the report signals generation timestamp plus nonce.
     * @param nonceHash The hash of the salt/nonce.
     */
    BlobGenerationResult generate(String requestHash, String timestampHash, String nonceHash);

    /**
     * Generates the desired attestation blob with provided fields for content binding
     *
     * @param flowName The work flow name to use for the blob generation request.
     * @param requestHash The hash of the request without attestation payload plus nonce.
     * @param timestampHash The hash of the report signals generation timestamp plus nonce.
     * @param nonceHash The hash of the salt/nonce.
     */
    default BlobGenerationResult generate(
            String flowName, String requestHash, String timestampHash, String nonceHash) {
        return new BlobGenerationResult(
                /* attestationBlob= */ "", /* errorMessage= */ "Method not implemented");
    }
}
