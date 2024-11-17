// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.data_sharing.member_role.MemberRole;

/**
 * Java side of the JNI bridge between CollaborationServiceImpl in Java and C++. All method calls
 * are delegated to the native C++ class.
 */
@JNINamespace("collaboration")
public class CollaborationServiceImpl implements CollaborationService {
    private long mNativePtr;

    @CalledByNative
    private static CollaborationServiceImpl create(long nativePtr) {
        return new CollaborationServiceImpl(nativePtr);
    }

    private CollaborationServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public boolean isEmptyService() {
        return CollaborationServiceImplJni.get().isEmptyService(mNativePtr, this);
    }

    @Override
    public ServiceStatus getServiceStatus() {
        return CollaborationServiceImplJni.get().getServiceStatus(mNativePtr);
    }

    @Override
    public @MemberRole int getCurrentUserRoleForGroup(String groupId) {
        return CollaborationServiceImplJni.get().getCurrentUserRoleForGroup(mNativePtr, groupId);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        boolean isEmptyService(
                long nativeCollaborationServiceAndroid, CollaborationServiceImpl caller);

        ServiceStatus getServiceStatus(long nativeCollaborationServiceAndroid);

        int getCurrentUserRoleForGroup(long nativeCollaborationServiceAndroid, String groupId);
    }
}
