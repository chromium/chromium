// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresExtension;

import org.chromium.net.ConnectionCloseSource;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidQuicExceptionWrapper extends org.chromium.net.QuicException {
    private final AndroidNetworkExceptionWrapper mBackend;

    AndroidQuicExceptionWrapper(android.net.http.QuicException backend) {
        super(backend.getMessage(), backend);
        this.mBackend = new AndroidNetworkExceptionWrapper(backend, true);
    }

    @Override
    public int getQuicDetailedErrorCode() {
        // TODO(danstahr): hidden API
        return 0;
    }

    @Override
    public int getErrorCode() {
        return mBackend.getErrorCode();
    }

    @Override
    public int getCronetInternalErrorCode() {
        return mBackend.getCronetInternalErrorCode();
    }

    @Override
    public boolean immediatelyRetryable() {
        return mBackend.immediatelyRetryable();
    }

    @Override
    public @ConnectionCloseSource int getConnectionCloseSource() {
        // Not available in HTTP Engine.
        return ConnectionCloseSource.UNKNOWN;
    }
}
