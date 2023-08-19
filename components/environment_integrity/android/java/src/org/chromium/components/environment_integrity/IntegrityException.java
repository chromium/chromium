// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.environment_integrity;

import org.chromium.components.environment_integrity.enums.IntegrityResponse;
/**
 * Exceptions for calling the Environment Integrity API.
 * Allows passing known error codes back to the browser.
 */
public class IntegrityException extends RuntimeException {
    @IntegrityResponse
    private final int mErrorCode;

    public IntegrityException(String message, @IntegrityResponse int errorCode, Throwable cause) {
        super(message, cause);
        this.mErrorCode = errorCode;
    }

    public IntegrityException(String message, @IntegrityResponse int errorCode) {
        this(message, errorCode, null);
    }

    /**
     * @return Error code from EnvironmentIntegrity API.
     */
    @IntegrityResponse
    public int getErrorCode() {
        return mErrorCode;
    }
}
