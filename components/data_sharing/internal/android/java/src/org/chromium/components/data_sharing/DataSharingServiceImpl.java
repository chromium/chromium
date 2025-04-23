// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.data_sharing.mojom.LogSource;
import org.chromium.url.GURL;

/**
 * Java side of the JNI bridge between DataSharingServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("data_sharing")
@NullMarked
public class DataSharingServiceImpl implements DataSharingService {
    private long mNativePtr;

    private final UserDataHost mUserDataHost = new UserDataHost();
    private final ObserverBridge mObserverBridge = new ObserverBridge();
    private final Logger mLogger = new LoggerImpl();

    private class LoggerImpl implements Logger {
        @Override
        public void log(@LogSource.EnumType int source, String message) {
            DataSharingServiceImplJni.get().log(mNativePtr, source, message);
        }
    }

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
    public void readGroup(String groupId, Callback<GroupDataOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get().readGroup(mNativePtr, groupId, callback);
    }

    @Override
    public void createGroup(String groupName, Callback<GroupDataOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get().createGroup(mNativePtr, groupName, callback);
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
    public GURL getDataSharingUrl(GroupData groupData) {
        GroupToken groupToken = groupData.groupToken;
        return DataSharingServiceImplJni.get()
                .getDataSharingUrl(
                        mNativePtr,
                        groupToken.collaborationId,
                        assumeNonNull(groupToken.accessToken));
    }

    @Override
    public DataSharingService.ParseUrlResult parseDataSharingUrl(GURL url) {
        return DataSharingServiceImplJni.get().parseDataSharingUrl(mNativePtr, url);
    }

    @Override
    public void ensureGroupVisibility(
            String groupId, Callback<GroupDataOrFailureOutcome> callback) {
        DataSharingServiceImplJni.get().ensureGroupVisibility(mNativePtr, groupId, callback);
    }

    @Override
    public void getSharedEntitiesPreview(
            GroupToken groupToken, Callback<SharedDataPreviewOrFailureOutcome> callback) {
        if (sSharedEntitiesPreviewForTesting != null) {
            callback.onResult(sSharedEntitiesPreviewForTesting);
            return;
        }
        DataSharingServiceImplJni.get()
                .getSharedEntitiesPreview(
                        mNativePtr,
                        groupToken.collaborationId,
                        assumeNonNull(groupToken.accessToken),
                        callback);
    }

    @Override
    public DataSharingUIDelegate getUiDelegate() {
        return DataSharingServiceImplJni.get().getUiDelegate(mNativePtr);
    }

    @Override
    public Logger getLogger() {
        return mLogger;
    }

    /* Sets a test preview data to return for all preview requests. */
    public void setSharedEntitiesPreviewForTesting(String groupId) {
        DataSharingServiceImplJni.get().setSharedEntitiesPreviewForTesting(mNativePtr, groupId);
    }

    private static @Nullable SharedDataPreviewOrFailureOutcome sSharedEntitiesPreviewForTesting;

    /**
     * TODO(ssid): Deprecate this method. Sets a test preview data to return for all preview
     * requests.
     */
    public static void setSharedEntitiesPreviewForTesting(
            SharedDataPreviewOrFailureOutcome preview) {
        sSharedEntitiesPreviewForTesting = preview;
        ResettersForTesting.register(() -> sSharedEntitiesPreviewForTesting = null);
    }

    /** Static utility to get the data sharing URL for testing. */
    public static GURL getDataSharingUrlForTesting(GroupToken groupToken) {
        return DataSharingServiceImplJni.get()
                .getDataSharingUrlForTesting(
                        groupToken.groupId, assumeNonNull(groupToken.accessToken));
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
        mUserDataHost.destroy();
    }

    @NativeMethods
    interface Natives {
        void readGroup(
                long nativeDataSharingServiceAndroid,
                String groupId,
                Callback<GroupDataOrFailureOutcome> callback);

        void createGroup(
                long nativeDataSharingServiceAndroid,
                String groupName,
                Callback<GroupDataOrFailureOutcome> callback);

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

        GURL getDataSharingUrl(
                long nativeDataSharingServiceAndroid, String groupId, String accessToken);

        DataSharingService.ParseUrlResult parseDataSharingUrl(
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

        DataSharingUIDelegate getUiDelegate(long nativeDataSharingServiceAndroid);

        void log(long nativeDataSharingServiceAndroid, int source, String message);

        GURL getDataSharingUrlForTesting(String groupId, String accessToken);

        void setSharedEntitiesPreviewForTesting(
                long nativeDataSharingServiceAndroid, String groupId);
    }
}
