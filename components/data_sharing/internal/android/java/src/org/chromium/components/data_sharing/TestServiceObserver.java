// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Test observer that registers itself to the Java service, and sends notification to the native
 * test.
 *
 * <p>See data_sharing_service_android_unittest.cc for usage.
 */
@JNINamespace("data_sharing")
public class TestServiceObserver implements DataSharingService.Observer {
    private final long mNativePtr;
    private int mOnGroupChangeCount;
    private int mOnGroupAddedCount;
    private int mOnGroupRemovedCount;

    public TestServiceObserver(long nativePtr) {
        this.mNativePtr = nativePtr;
        this.mOnGroupChangeCount = 0;
        this.mOnGroupAddedCount = 0;
        this.mOnGroupRemovedCount = 0;
    }

    @CalledByNative
    public static TestServiceObserver createAndAdd(DataSharingService service, long nativePtr) {
        TestServiceObserver obs = new TestServiceObserver(nativePtr);
        service.addObserver(obs);
        return obs;
    }

    @CalledByNative
    public void destroy(DataSharingService service) {
        service.removeObserver(this);
    }

    @CalledByNative
    private int getOnGroupChangeCount() {
        return mOnGroupChangeCount;
    }

    @CalledByNative
    private int getOnGroupAddedCount() {
        return mOnGroupAddedCount;
    }

    @CalledByNative
    private int getOnGroupRemovedCount() {
        return mOnGroupRemovedCount;
    }

    @Override
    public void onGroupChanged(GroupData groupData) {
        mOnGroupChangeCount++;
        TestServiceObserverJni.get().onObserverNotify(mNativePtr);
    }

    @Override
    public void onGroupAdded(GroupData groupData) {
        mOnGroupAddedCount++;
        TestServiceObserverJni.get().onObserverNotify(mNativePtr);
    }

    @Override
    public void onGroupRemoved(String groupId) {
        mOnGroupRemovedCount++;
        TestServiceObserverJni.get().onObserverNotify(mNativePtr);
    }

    @NativeMethods
    interface Natives {
        void onObserverNotify(long observerPtr);
    }
}
