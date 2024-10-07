// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ThreadUtils;

/** A companion object to the native MessagingBackendServiceBridgeTest. */
@JNINamespace("tab_groups::messaging")
public class MessagingBackendServiceBridgeUnitTestCompanion {
    // The service instance we're testing.
    private MessagingBackendService mService;

    private MessagingBackendService.PersistentMessageObserver mObserver =
            mock(MessagingBackendService.PersistentMessageObserver.class);

    @CalledByNative
    private MessagingBackendServiceBridgeUnitTestCompanion(MessagingBackendService service) {
        mService = service;
        ThreadUtils.setUiThread(Looper.getMainLooper());
    }

    @CalledByNative
    private boolean isInitialized() {
        return mService.isInitialized();
    }

    @CalledByNative
    private void addPersistentMessageObserver() {
        mService.addPersistentMessageObserver(mObserver);
    }

    @CalledByNative
    private void removePersistentMessageObserver() {
        mService.removePersistentMessageObserver(mObserver);
    }

    @CalledByNative
    private void verifyOnInitializedCalled(int times) {
        verify(mObserver, times(times)).onMessagingBackendServiceInitialized();
    }
}
