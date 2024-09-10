// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.url.GURL;

/**
 * Java side of the JNI bridge between DataSharingServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("data_sharing")
public class DataSharingServiceImpl implements DataSharingService {
    private long mNativePtr;

    private final UserDataHost mUserDataHost = new UserDataHost();
    private final ObserverBridge mObserverBridge = new ObserverBridge();

    @CalledByNative
    private static DataSharingServiceImpl create(long nativePtr) {
        return new DataSharingServiceImpl(nativePtr);
    }

    private DataSharingServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    private ObserverBridge getObserverBridge() {
        return mObserverBridge;
    }

    @Override
    public void addObserver(Observer observer) {
        mObserverBridge.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObserverBridge.removeObserver(observer);
    }

    @Override
    public void readAllGroups(Callback<GroupsDataSetOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get().readAllGroups(mNativePtr, callback);
    }

    @Override
    public void readGroup(String groupId, Callback<GroupDataOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get().readGroup(mNativePtr, groupId, callback);
    }

    @Override
    public void createGroup(String groupName, Callback<GroupDataOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get().createGroup(mNativePtr, groupName, callback);
    }

    @Override
    public void deleteGroup(String groupId, Callback<Integer> callback) {
        DataSharingServiceImplJni.get().deleteGroup(mNativePtr, groupId, callback);
    }

    @Override
    public void inviteMember(String groupId, String inviteeEmail, Callback<Integer> callback) {
        DataSharingServiceImplJni.get().inviteMember(mNativePtr, groupId, inviteeEmail, callback);
    }

    @Override
    public void addMember(String groupId, String accessToken, Callback<Integer> callback) {
        DataSharingServiceImplJni.get().addMember(mNativePtr, groupId, accessToken, callback);
    }

    @Override
    public void removeMember(String groupId, String memberEmail, Callback<Integer> callback) {
        DataSharingServiceImplJni.get().removeMember(mNativePtr, groupId, memberEmail, callback);
    }

    @Override
    public boolean isEmptyService() {
        return DataSharingServiceImplJni.get().isEmptyService(mNativePtr, this);
    }

    @Override
    public DataSharingNetworkLoader getNetworkLoader() {
        return DataSharingServiceImplJni.get().getNetworkLoader(mNativePtr);
    }

    @Override
    public UserDataHost getUserDataHost() {
        return mUserDataHost;
    }

    @Override
    public GURL getDataSharingURL(GroupData groupData) {
        return DataSharingServiceImplJni.get()
                .getDataSharingURL(
                        mNativePtr, groupData.groupToken.groupId, groupData.groupToken.accessToken);
    }

    @Override
    public DataSharingService.ParseURLResult parseDataSharingURL(GURL url) {
        return DataSharingServiceImplJni.get().parseDataSharingURL(mNativePtr, url);
    }

    @Override
    public void ensureGroupVisibility(
            String groupId, Callback<GroupDataOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get().ensureGroupVisibility(mNativePtr, groupId, callback);
    }

    @Override
    public void getSharedEntitiesPreview(
            GroupToken groupToken, Callback<SharedDataPreviewOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get()
                .getSharedEntitiesPreview(
                        mNativePtr, groupToken.groupId, groupToken.accessToken, callback);
    }

    @Override
    public DataSharingUIDelegate getUIDelegate() {
        return DataSharingServiceImplJni.get().getUIDelegate(mNativePtr);
    }

    @Override
    public ServiceStatus getServiceStatus() {
        return DataSharingServiceImplJni.get().getServiceStatus(mNativePtr);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
        mUserDataHost.destroy();
    }

    @NativeMethods
    interface Natives {
        void readAllGroups(
                long nativeDataSharingServiceAndroid,
                Callback<GroupsDataSetOrFailureOutcome> callback);

        void readGroup(
                long nativeDataSharingServiceAndroid,
                String groupId,
                Callback<GroupDataOrFailureOutcome> callback);

        void createGroup(
                long nativeDataSharingServiceAndroid,
                String groupName,
                Callback<GroupDataOrFailureOutcome> callback);

        void deleteGroup(
                long nativeDataSharingServiceAndroid, String groupId, Callback<Integer> callback);

        void inviteMember(
                long nativeDataSharingServiceAndroid,
                String groupId,
                String inviteeEmail,
                Callback<Integer> callback);

        void addMember(
                long nativeDataSharingServiceAndroid,
                String groupId,
                String accessToken,
                Callback<Integer> callback);

        void removeMember(
                long nativeDataSharingServiceAndroid,
                String groupId,
                String memberEmail,
                Callback<Integer> callback);

        boolean isEmptyService(long nativeDataSharingServiceAndroid, DataSharingServiceImpl caller);

        DataSharingNetworkLoader getNetworkLoader(long nativeDataSharingServiceAndroid);

        GURL getDataSharingURL(
                long nativeDataSharingServiceAndroid, String groupId, String accessToken);

        DataSharingService.ParseURLResult parseDataSharingURL(
                long nativeDataSharingServiceAndroid, GURL url);

        void ensureGroupVisibility(
                long nativeDataSharingServiceAndroid,
                String groupId,
                Callback<GroupDataOrFailureOutcome> callback);

        void getSharedEntitiesPreview(
                long nativeDataSharingServiceAndroid,
                String groupId,
                String accessToken,
                Callback<SharedDataPreviewOrFailureOutcome> callback);

        DataSharingUIDelegate getUIDelegate(long nativeDataSharingServiceAndroid);

        ServiceStatus getServiceStatus(long nativeDataSharingServiceAndroid);
    }
}
