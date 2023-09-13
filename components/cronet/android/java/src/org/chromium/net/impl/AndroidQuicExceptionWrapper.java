// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

@RequiresApi(api = 34)
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
}
