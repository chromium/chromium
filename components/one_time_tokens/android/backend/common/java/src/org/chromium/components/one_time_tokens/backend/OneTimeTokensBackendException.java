// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.one_time_tokens.backend;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;

/** Exception thrown when there is an error in the one-time token backend. */
@NullMarked
public class OneTimeTokensBackendException extends Exception {
    private final @OneTimeTokensBackendErrorCode int mErrorCode;

    public OneTimeTokensBackendException(
            @Nullable String message, @OneTimeTokensBackendErrorCode int errorCode) {
        super(message);
        mErrorCode = errorCode;
    }

    public @OneTimeTokensBackendErrorCode int getErrorCode() {
        return mErrorCode;
    }
}
