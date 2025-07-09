// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.comments;

import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ThreadUtils;

/** A companion object to the native CommentsServiceBridgeTest. */
@JNINamespace("collaboration::comments::android")
public class CommentsServiceBridgeUnitTestCompanion {
    private final CommentsService mService;

    @CalledByNative
    private CommentsServiceBridgeUnitTestCompanion(CommentsService service) {
        mService = service;
        ThreadUtils.setUiThread(Looper.getMainLooper());
    }

    @CalledByNative
    private boolean isInitialized() {
        return mService.isInitialized();
    }

    @CalledByNative
    private boolean isEmptyService() {
        return mService.isEmptyService();
    }
}
