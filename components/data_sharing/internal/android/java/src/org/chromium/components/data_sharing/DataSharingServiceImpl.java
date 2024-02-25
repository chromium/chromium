// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Java side of the JNI bridge between DataSharingServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("data_sharing")
public class DataSharingServiceImpl implements DataSharingService {
    private long mNativePtr;

    @CalledByNative
    private static DataSharingServiceImpl create(long nativePtr) {
        return new DataSharingServiceImpl(nativePtr);
    }

    private DataSharingServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public boolean isEmptyService() {
        return DataSharingServiceImplJni.get().isEmptyService(mNativePtr, this);
    }

    @Override
    public DataSharingNetworkLoader getNetworkLoader() {
        return DataSharingServiceImplJni.get().getNetworkLoader(mNativePtr);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        boolean isEmptyService(long nativeDataSharingServiceAndroid, DataSharingServiceImpl caller);

        DataSharingNetworkLoader getNetworkLoader(long nativeDataSharingServiceAndroid);
    }
}
