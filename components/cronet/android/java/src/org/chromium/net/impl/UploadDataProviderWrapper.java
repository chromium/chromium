// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

import java.io.IOException;
import java.nio.ByteBuffer;

@RequiresApi(api = 34)
@SuppressWarnings("Override")
class UploadDataProviderWrapper extends android.net.http.UploadDataProvider {
    private final org.chromium.net.UploadDataProvider mBackend;

    public UploadDataProviderWrapper(org.chromium.net.UploadDataProvider backend) {
        this.mBackend = backend;
    }

    @Override
    public long getLength() throws IOException {
        return mBackend.getLength();
    }

    @Override
    public void read(android.net.http.UploadDataSink uploadDataSink, ByteBuffer byteBuffer)
            throws IOException {
        AndroidUploadDataSinkWrapper wrapper = new AndroidUploadDataSinkWrapper(uploadDataSink);
        mBackend.read(wrapper, byteBuffer);
    }

    @Override
    public void rewind(android.net.http.UploadDataSink uploadDataSink) throws IOException {
        AndroidUploadDataSinkWrapper wrapper = new AndroidUploadDataSinkWrapper(uploadDataSink);
        mBackend.rewind(wrapper);
    }

    @Override
    public void close() throws IOException {
        mBackend.close();
    }
}
