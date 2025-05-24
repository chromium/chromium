// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Information about a group. */
@JNINamespace("data_sharing")
@NullMarked
public class DataSharingNetworkResult {
    public final byte[] resultBytes;
    public final @NetworkLoaderStatus int status;
    public final int networkErrorCode;

    DataSharingNetworkResult(byte[] resultBytes,
                            @NetworkLoaderStatus int status,
                            int networkErrorCode) {
        this.resultBytes = resultBytes;
        this.status = status;
        this.networkErrorCode = networkErrorCode;
    }

    @CalledByNative
    private static DataSharingNetworkResult createDataSharingNetworkResult(
            byte[] resultBytes, @NetworkLoaderStatus int status, int networkErrorCode) {
        return new DataSharingNetworkResult(resultBytes, status, networkErrorCode);
    }
}
