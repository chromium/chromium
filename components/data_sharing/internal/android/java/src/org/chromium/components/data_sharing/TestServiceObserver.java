// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Test observer that registers itself to the Java service, and sends notification to the native
 * test.
 *
 * <p>See data_sharing_service_android_unittest.cc for usage.
 */
@JNINamespace("data_sharing")
@NullMarked
public class TestServiceObserver implements DataSharingService.Observer {
    @JniType("::data_sharing::TestJavaObserver")
    interface NativeTestJavaObserver extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeTestJavaObserver> mNativePtr;
    private int mOnGroupChangeCount;
    private int mOnGroupAddedCount;
    private int mOnGroupRemovedCount;

    private TestServiceObserver(JniRawPtr<NativeTestJavaObserver> nativePtr) {
        this.mNativePtr = nativePtr;
        this.mOnGroupChangeCount = 0;
        this.mOnGroupAddedCount = 0;
        this.mOnGroupRemovedCount = 0;
    }

    @CalledByNative
    static TestServiceObserver createAndAdd(
            DataSharingService service, JniRawPtr<NativeTestJavaObserver> nativePtr) {
        TestServiceObserver obs = new TestServiceObserver(nativePtr);
        service.addObserver(obs);
        return obs;
    }

    @CalledByNative
    void destroy(DataSharingService service) {
        service.removeObserver(this);
        if (mNativePtr != null) {
            mNativePtr.release();
            mNativePtr = null;
        }
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
        if (mNativePtr != null) {
            TestServiceObserverJni.get().onObserverNotify(mNativePtr);
        }
    }

    @Override
    public void onGroupAdded(GroupData groupData) {
        mOnGroupAddedCount++;
        if (mNativePtr != null) {
            TestServiceObserverJni.get().onObserverNotify(mNativePtr);
        }
    }

    @Override
    public void onGroupRemoved(String groupId) {
        mOnGroupRemovedCount++;
        if (mNativePtr != null) {
            TestServiceObserverJni.get().onObserverNotify(mNativePtr);
        }
    }

    @NativeMethods
    interface Natives {
        void onObserverNotify(JniPtr<NativeTestJavaObserver> self);
    }
}
