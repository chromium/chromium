// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.url.GURL;

/**
 * Java side of the JNI bridge between DataSharingNetworkLoaderImpl in Java and C++. All method
 * calls are delegated to the native C++ class.
 */
@JNINamespace("data_sharing")
public class DataSharingNetworkLoaderImpl implements DataSharingNetworkLoader {
    private long mNativePtr;


    @CalledByNative
    private static DataSharingNetworkLoaderImpl create(long nativePtr) {
        return new DataSharingNetworkLoaderImpl(nativePtr);
    }

    DataSharingNetworkLoaderImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @Override
    public void loadUrl(
            GURL url,
            String[] scopes,
            byte[] postData,
            @DataSharingRequestType int requestType,
            Callback<DataSharingNetworkResult> callback) {
        ThreadUtils.postOnUiThread(
                () -> {
                    loadUrlOnUiThread(
                            url,
                            scopes,
                            postData,
                            DataSharingNetworkUtils.getNetworkTrafficAnnotationTag(requestType),
                            callback);
                });
    }

    private void loadUrlOnUiThread(
            GURL url,
            String[] scopes,
            byte[] postData,
            NetworkTrafficAnnotationTag networkAnnotationTag,
            Callback<DataSharingNetworkResult> callback) {
        if (mNativePtr != 0) {
            DataSharingNetworkLoaderImplJni.get()
                    .loadUrl(
                            mNativePtr,
                            url,
                            scopes,
                            postData,
                            networkAnnotationTag.getHashCode(),
                            callback);
        }
    }

    @NativeMethods
    interface Natives {
        void loadUrl(
                long nativeDataSharingNetworkLoaderAndroid,
                GURL url,
                String[] scopes,
                byte[] postData,
                int annotationHashCode,
                Callback<DataSharingNetworkResult> callback);
    }
}
