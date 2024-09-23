// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Helper class for testing that provides functionality for setting bridge {@link
 * DataSharingSDKDelegateBridge} over JNI with test implementation of {@link
 * DataSharingSDKDelegate}.
 */
@JNINamespace("data_sharing")
public class DataSharingSDKDelegateAndroidTestSupport {

    @CalledByNative
    private static DataSharingSDKDelegate createDelegateTestImpl() {
        return new DataSharingSDKDelegateTestImpl();
    }
}
