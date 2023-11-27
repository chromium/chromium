// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresExtension;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidNetworkExceptionWrapper extends org.chromium.net.NetworkException {
    private final android.net.http.NetworkException mBackend;

    AndroidNetworkExceptionWrapper(android.net.http.NetworkException backend) {
        this(backend, false);
    }

    AndroidNetworkExceptionWrapper(
            android.net.http.NetworkException backend, boolean expectQuicException) {
        super(backend.getMessage(), backend);
        this.mBackend = backend;

        if (!expectQuicException && backend instanceof android.net.http.QuicException) {
            throw new IllegalArgumentException(
                    "Translating QuicException as NetworkException results in loss of information. "
                            + "Make sure you handle QuicException first. See the stacktrace "
                            + "for where the translation is being performed, and the cause "
                            + "for the exception being translated.",
                    backend);
        }
    }

    @Override
    public int getErrorCode() {
        return mBackend.getErrorCode();
    }

    @Override
    public int getCronetInternalErrorCode() {
        // TODO(danstahr): Hidden API
        return -1;
    }

    @Override
    public boolean immediatelyRetryable() {
        return mBackend.isImmediatelyRetryable();
    }
}
