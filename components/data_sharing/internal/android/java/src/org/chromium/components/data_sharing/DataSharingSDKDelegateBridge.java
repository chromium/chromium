// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.data_sharing.DataSharingSDKDelegateProtoResponseCallback.Status;
import org.chromium.components.data_sharing.protocol.AddAccessTokenParams;
import org.chromium.components.data_sharing.protocol.AddAccessTokenResult;
import org.chromium.components.data_sharing.protocol.AddMemberParams;
import org.chromium.components.data_sharing.protocol.CreateGroupParams;
import org.chromium.components.data_sharing.protocol.CreateGroupResult;
import org.chromium.components.data_sharing.protocol.DeleteGroupParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailResult;
import org.chromium.components.data_sharing.protocol.ReadGroupsParams;
import org.chromium.components.data_sharing.protocol.ReadGroupsResult;
import org.chromium.components.data_sharing.protocol.RemoveMemberParams;

/** Java counterpart to the C++ DataSharingSDKDelegateAndroid class. */
@JNINamespace("data_sharing")
public class DataSharingSDKDelegateBridge {

    private DataSharingSDKDelegate mSDKDelegateImpl;

    @CalledByNative
    private static DataSharingSDKDelegateBridge create(
            long nativePtr, DataSharingSDKDelegate delegate) {
        return new DataSharingSDKDelegateBridge(nativePtr, delegate);
    }

    private DataSharingSDKDelegateBridge(long nativePtr, DataSharingSDKDelegate delegate) {
        mSDKDelegateImpl = delegate;
    }

    @CalledByNative
    private void clearNativePtr() {}

    @CalledByNative
    public void initialize(DataSharingNetworkLoader networkLoader) {
        mSDKDelegateImpl.initialize(networkLoader);
    }

    @CalledByNative
    public void createGroup(String protoParams, long nativeCallbackPtr) {
        CreateGroupParams params;
        try {
            params = CreateGroupParams.parseFrom(protoParams.getBytes());
        } catch (InvalidProtocolBufferException e) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        DataSharingSDKDelegateBridgeJni.get()
                                .runCreateGroupCallback(
                                        nativeCallbackPtr,
                                        CreateGroupResult.newBuilder().build().toByteArray(),
                                        Status.FAILURE);
                    });
            return;
        }
        mSDKDelegateImpl.createGroup(
                params,
                (byte[] serializedProto, int status) -> {
                    DataSharingSDKDelegateBridgeJni.get()
                            .runCreateGroupCallback(nativeCallbackPtr, serializedProto, status);
                });
    }

    @CalledByNative
    public void readGroups(String protoParams, long nativeCallbackPtr) {
        ReadGroupsParams params;
        try {
            params = ReadGroupsParams.parseFrom(protoParams.getBytes());
        } catch (InvalidProtocolBufferException e) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        DataSharingSDKDelegateBridgeJni.get()
                                .runReadGroupsCallback(
                                        nativeCallbackPtr,
                                        ReadGroupsResult.newBuilder().build().toByteArray(),
                                        Status.FAILURE);
                    });
            return;
        }
        mSDKDelegateImpl.readGroups(
                params,
                (byte[] serializedProto, int status) -> {
                    DataSharingSDKDelegateBridgeJni.get()
                            .runReadGroupsCallback(nativeCallbackPtr, serializedProto, status);
                });
    }

    @CalledByNative
    public void addMember(String protoParams, long nativeCallbackPtr) {
        AddMemberParams params;
        try {
            params = AddMemberParams.parseFrom(protoParams.getBytes());
        } catch (InvalidProtocolBufferException e) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        DataSharingSDKDelegateBridgeJni.get()
                                .runGetStatusCallback(nativeCallbackPtr, Status.FAILURE);
                    });
            return;
        }
        mSDKDelegateImpl.addMember(
                params,
                (Integer status) ->
                        DataSharingSDKDelegateBridgeJni.get()
                                .runGetStatusCallback(nativeCallbackPtr, status));
    }

    @CalledByNative
    public void removeMember(String protoParams, long nativeCallbackPtr) {
        RemoveMemberParams params;
        try {
            params = RemoveMemberParams.parseFrom(protoParams.getBytes());
        } catch (InvalidProtocolBufferException e) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        DataSharingSDKDelegateBridgeJni.get()
                                .runGetStatusCallback(nativeCallbackPtr, Status.FAILURE);
                    });
            return;
        }
        mSDKDelegateImpl.removeMember(
                params,
                (Integer status) ->
                        DataSharingSDKDelegateBridgeJni.get()
                                .runGetStatusCallback(nativeCallbackPtr, status));
    }

    @CalledByNative
    public void deleteGroup(String protoParams, long nativeCallbackPtr) {
        DeleteGroupParams params;
        try {
            params = DeleteGroupParams.parseFrom(protoParams.getBytes());
        } catch (InvalidProtocolBufferException e) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        DataSharingSDKDelegateBridgeJni.get()
                                .runGetStatusCallback(nativeCallbackPtr, Status.FAILURE);
                    });
            return;
        }
        mSDKDelegateImpl.deleteGroup(
                params,
                (Integer status) ->
                        DataSharingSDKDelegateBridgeJni.get()
                                .runGetStatusCallback(nativeCallbackPtr, status));
    }

    @CalledByNative
    public void lookupGaiaIdByEmail(String protoParams, long nativeCallbackPtr) {
        LookupGaiaIdByEmailParams params;
        try {
            params = LookupGaiaIdByEmailParams.parseFrom(protoParams.getBytes());
        } catch (InvalidProtocolBufferException e) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        DataSharingSDKDelegateBridgeJni.get()
                                .runLookupGaiaIdByEmailCallback(
                                        nativeCallbackPtr,
                                        LookupGaiaIdByEmailResult.newBuilder()
                                                .build()
                                                .toByteArray(),
                                        Status.FAILURE);
                    });
            return;
        }

        mSDKDelegateImpl.lookupGaiaIdByEmail(
                params,
                (byte[] serializedProto, int status) -> {
                    DataSharingSDKDelegateBridgeJni.get()
                            .runLookupGaiaIdByEmailCallback(
                                    nativeCallbackPtr, serializedProto, status);
                });
    }

    @CalledByNative
    public void addAccessToken(String protoParams, long nativeCallbackPtr) {
        AddAccessTokenParams params;
        try {
            params = AddAccessTokenParams.parseFrom(protoParams.getBytes());
        } catch (InvalidProtocolBufferException e) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        DataSharingSDKDelegateBridgeJni.get()
                                .runAddAccessTokenCallback(
                                        nativeCallbackPtr,
                                        AddAccessTokenResult.newBuilder().build().toByteArray(),
                                        Status.FAILURE);
                    });
            return;
        }
        mSDKDelegateImpl.addAccessToken(
                params,
                (byte[] serializedProto, int status) -> {
                    DataSharingSDKDelegateBridgeJni.get()
                            .runAddAccessTokenCallback(nativeCallbackPtr, serializedProto, status);
                });
    }

    /**
     * Set a {@DataSharingSDKDelegate} to use for testing. All subsequent calls will return the test
     * object rather than the real object.
     */
    void overrideDelegateForTesting(DataSharingSDKDelegate delegate) {
        DataSharingSDKDelegate old = mSDKDelegateImpl;
        ResettersForTesting.register(() -> mSDKDelegateImpl = old);
        mSDKDelegateImpl = delegate;
    }

    @NativeMethods
    interface Natives {
        void runCreateGroupCallback(long callback, byte[] serializedProto, int status);

        void runReadGroupsCallback(long callback, byte[] serializedProto, int status);

        void runGetStatusCallback(long callback, int status);

        void runLookupGaiaIdByEmailCallback(long callback, byte[] serializedProto, int status);

        void runAddAccessTokenCallback(long callback, byte[] serializedProto, int status);
    }
}
