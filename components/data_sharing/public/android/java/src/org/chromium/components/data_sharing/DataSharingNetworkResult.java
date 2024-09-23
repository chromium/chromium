// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Information about a group. */
@JNINamespace("data_sharing")
public class DataSharingNetworkResult {
    public final byte[] resultBytes;
    public final @NetworkLoaderStatus int status;

    DataSharingNetworkResult(byte[] resultBytes, @NetworkLoaderStatus int status) {
        this.resultBytes = resultBytes;
        this.status = status;
    }

    @CalledByNative
    private static DataSharingNetworkResult createDataSharingNetworkResult(
            byte[] resultBytes, @NetworkLoaderStatus int status) {
        return new DataSharingNetworkResult(resultBytes, status);
    }
}
