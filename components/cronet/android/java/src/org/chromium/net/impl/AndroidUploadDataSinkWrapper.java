// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

@RequiresApi(api = 34)
class AndroidUploadDataSinkWrapper extends org.chromium.net.UploadDataSink {
    private final android.net.http.UploadDataSink mBackend;

    AndroidUploadDataSinkWrapper(android.net.http.UploadDataSink backend) {
        this.mBackend = backend;
    }

    @Override
    public void onReadSucceeded(boolean finalChunk) {
        mBackend.onReadSucceeded(finalChunk);
    }

    @Override
    public void onReadError(Exception exception) {
        mBackend.onReadError(exception);
    }

    @Override
    public void onRewindSucceeded() {
        mBackend.onRewindSucceeded();
    }

    @Override
    public void onRewindError(Exception exception) {
        mBackend.onRewindError(exception);
    }
}
