// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.device_attestation;

import org.chromium.build.annotations.NullMarked;

/** Delegate for attestation blob generation functions implemented downstream for Google Chrome. */
@NullMarked
public interface AttestationBlobGeneratorDelegate {
    /**
     * Generates the desired attestation blob with provided fields for content binding
     *
     * @param requestHash The hash of the report request without attestation payload plus nonce.
     * @param timestampHash The hash of the report signals generation timestamp plus nonce.
     * @param nonceHash The hash of the salt/nonce.
     */
    BlobGenerationResult generate(String requestHash, String timestampHash, String nonceHash);
}
