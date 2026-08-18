// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresExtension;

import org.chromium.net.ConnectionCloseSource;

// Note we specify both RequiresApi and RequiresExtension because some older linters may only
// recognize the former.
@RequiresApi(EXT_API_LEVEL)
@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidQuicExceptionWrapper extends org.chromium.net.QuicException {
    private final AndroidNetworkExceptionWrapper mBackend;

    AndroidQuicExceptionWrapper(android.net.http.QuicException backend) {
        // Some Cronet users rely on the specific structure of native Cronet exceptions, especially
        // getCause(), which they expect to be null. For this reason, we cannot naively expose a
        // causal chain of wrapped exceptions. We expose a null cause instead, and attach the
        // backend exception as a suppressed exception so the full causal chain is preserved for
        // crash reporters and logging.
        super(backend.getMessage(), /* cause= */ null);
        this.mBackend = new AndroidNetworkExceptionWrapper(backend, true);
        AndroidNetworkExceptionWrapper.configureException(this, backend);
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
