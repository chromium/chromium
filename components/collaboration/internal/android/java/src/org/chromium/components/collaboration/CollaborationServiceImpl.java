// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.url.GURL;

/**
 * Java side of the JNI bridge between CollaborationServiceImpl in Java and C++. All method calls
 * are delegated to the native C++ class.
 */
@JNINamespace("collaboration")
@NullMarked
public class CollaborationServiceImpl implements CollaborationService {
    private final ObserverList<CollaborationService.Observer> mObservers = new ObserverList<>();
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
    public void startJoinFlow(
            CollaborationControllerDelegate delegate,
            GURL url,
            @CollaborationServiceJoinEntryPoint int entry) {
        CollaborationServiceImplJni.get()
                .startJoinFlow(mNativePtr, delegate.getNativePtr(), url, entry);
    }

    @Override
    public void startShareOrManageFlow(
            CollaborationControllerDelegate delegate,
            String syncId,
            @CollaborationServiceShareOrManageEntryPoint int entry) {
        CollaborationServiceImplJni.get()
                .startShareOrManageFlow(mNativePtr, delegate.getNativePtr(), syncId, entry);
    }

    @Override
    public ServiceStatus getServiceStatus() {
        return CollaborationServiceImplJni.get().getServiceStatus(mNativePtr);
    }

    @Override
    public @MemberRole int getCurrentUserRoleForGroup(@Nullable String collaborationId) {
        return CollaborationServiceImplJni.get()
                .getCurrentUserRoleForGroup(mNativePtr, collaborationId);
    }

    @Override
    public GroupData getGroupData(@Nullable String collaborationId) {
        return CollaborationServiceImplJni.get().getGroupData(mNativePtr, collaborationId);
    }

    @Override
    public void leaveGroup(String groupId, Callback<Boolean> callback) {
        CollaborationServiceImplJni.get().leaveGroup(mNativePtr, groupId, callback);
    }

    @Override
    public void deleteGroup(String groupId, Callback<Boolean> callback) {
        CollaborationServiceImplJni.get().deleteGroup(mNativePtr, groupId, callback);
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onServiceStatusChanged(ServiceStatus oldStatus, ServiceStatus newStatus) {
        for (CollaborationService.Observer observer : mObservers) {
            observer.onServiceStatusChanged(oldStatus, newStatus);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        boolean isEmptyService(
                long nativeCollaborationServiceAndroid, CollaborationServiceImpl caller);

        void startJoinFlow(
                long nativeCollaborationServiceAndroid,
                long delegateNativePtr,
                GURL url,
                int entry);

        void startShareOrManageFlow(
                long nativeCollaborationServiceAndroid,
                long delegateNativePtr,
                String syncId,
                int entry);

        ServiceStatus getServiceStatus(long nativeCollaborationServiceAndroid);

        int getCurrentUserRoleForGroup(
                long nativeCollaborationServiceAndroid, @Nullable String collaborationId);

        GroupData getGroupData(
                long nativeCollaborationServiceAndroid, @Nullable String collaborationId);

        void leaveGroup(
                long nativeCollaborationServiceAndroid, String groupId, Callback<Boolean> callback);

        void deleteGroup(
                long nativeCollaborationServiceAndroid, String groupId, Callback<Boolean> callback);
    }
}
