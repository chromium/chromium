// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_sdk_delegate_android.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/data_sharing/internal/android/data_sharing_network_loader_android.h"
#include "components/data_sharing/internal/jni_headers/DataSharingSDKDelegateBridge_jni.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/android/gurl_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace data_sharing {

// static
std::unique_ptr<DataSharingSDKDelegate> DataSharingSDKDelegate::CreateDelegate(
    ScopedJavaLocalRef<jobject> sdk_delegate) {
  return std::make_unique<DataSharingSDKDelegateAndroid>(sdk_delegate);
}

DataSharingSDKDelegateAndroid::DataSharingSDKDelegateAndroid(
    const JavaRef<jobject>& sdk_delegate) {
  JNIEnv* env = AttachCurrentThread();
  java_obj_.Reset(env, Java_DataSharingSDKDelegateBridge_create(
                           env, reinterpret_cast<int64_t>(this), sdk_delegate)
                           .obj());
}

DataSharingSDKDelegateAndroid::~DataSharingSDKDelegateAndroid() {
  Java_DataSharingSDKDelegateBridge_clearNativePtr(AttachCurrentThread(),
                                                   java_obj_);
}

ScopedJavaLocalRef<jobject> DataSharingSDKDelegateAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

void DataSharingSDKDelegateAndroid::Initialize(
    DataSharingNetworkLoader* data_sharing_network_loader) {
  JNIEnv* env = AttachCurrentThread();
  network_loader_ = std::make_unique<DataSharingNetworkLoaderAndroid>(
      data_sharing_network_loader);
  Java_DataSharingSDKDelegateBridge_initialize(
      env, java_obj_, network_loader_->GetJavaObject());
}
void DataSharingSDKDelegateAndroid::CreateGroup(
    const data_sharing_pb::CreateGroupParams& params,
    CreateGroupCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  std::string create_group_params;
  params.SerializeToString(&create_group_params);
  std::unique_ptr<CreateGroupCallback> wrapped_callback =
      std::make_unique<CreateGroupCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  Java_DataSharingSDKDelegateBridge_createGroup(
      env, java_obj_, ConvertUTF8ToJavaString(env, create_group_params),
      j_native_ptr);
  // We expect Java to always call us back through
  // JNI_DataSharingSDKDelegateBridge_RunCreateGroupCallback.
  wrapped_callback.release();
}

void DataSharingSDKDelegateAndroid::ReadGroups(
    const data_sharing_pb::ReadGroupsParams& params,
    ReadGroupsCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  std::string read_groups_params;
  params.SerializeToString(&read_groups_params);
  std::unique_ptr<ReadGroupsCallback> wrapped_callback =
      std::make_unique<ReadGroupsCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  Java_DataSharingSDKDelegateBridge_readGroups(
      env, java_obj_, ConvertUTF8ToJavaString(env, read_groups_params),
      j_native_ptr);
  // We expect Java to always call us back through
  // JNI_DataSharingSDKDelegateBridge_RunReadGroupsCallback.
  wrapped_callback.release();
}
void DataSharingSDKDelegateAndroid::AddMember(
    const data_sharing_pb::AddMemberParams& params,
    GetStatusCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  std::string add_member_params;
  params.SerializeToString(&add_member_params);
  std::unique_ptr<GetStatusCallback> wrapped_callback =
      std::make_unique<GetStatusCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  Java_DataSharingSDKDelegateBridge_addMember(
      env, java_obj_, ConvertUTF8ToJavaString(env, add_member_params),
      j_native_ptr);
  // We expect Java to always call us back through
  // JNI_DataSharingSDKDelegateBridge_RunAddMemberCallback.
  wrapped_callback.release();
}

void DataSharingSDKDelegateAndroid::RemoveMember(
    const data_sharing_pb::RemoveMemberParams& params,
    GetStatusCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  std::string remove_member_params;
  params.SerializeToString(&remove_member_params);
  std::unique_ptr<GetStatusCallback> wrapped_callback =
      std::make_unique<GetStatusCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  Java_DataSharingSDKDelegateBridge_removeMember(
      env, java_obj_, ConvertUTF8ToJavaString(env, remove_member_params),
      j_native_ptr);
  // We expect Java to always call us back through
  // JNI_DataSharingSDKDelegateBridge_RunRemoveMemberCallback.
  wrapped_callback.release();
}

void DataSharingSDKDelegateAndroid::DeleteGroup(
    const data_sharing_pb::DeleteGroupParams& params,
    GetStatusCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  std::string delete_group_params;
  params.SerializeToString(&delete_group_params);
  std::unique_ptr<GetStatusCallback> wrapped_callback =
      std::make_unique<GetStatusCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  Java_DataSharingSDKDelegateBridge_deleteGroup(
      env, java_obj_, ConvertUTF8ToJavaString(env, delete_group_params),
      j_native_ptr);
  // We expect Java to always call us back through
  // JNI_DataSharingSDKDelegateBridge_RunDeleteGroupCallback.
  wrapped_callback.release();
}

void DataSharingSDKDelegateAndroid::LookupGaiaIdByEmail(
    const data_sharing_pb::LookupGaiaIdByEmailParams& params,
    LookupGaiaIdByEmailCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  std::string lookup_gaid_id_params;
  params.SerializeToString(&lookup_gaid_id_params);
  std::unique_ptr<LookupGaiaIdByEmailCallback> wrapped_callback =
      std::make_unique<LookupGaiaIdByEmailCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  Java_DataSharingSDKDelegateBridge_lookupGaiaIdByEmail(
      env, java_obj_, ConvertUTF8ToJavaString(env, lookup_gaid_id_params),
      j_native_ptr);
  // We expect Java to always call us back through
  // JNI_DataSharingSDKDelegateBridge_RunLookupGaiaIdByEmailCallback.
  wrapped_callback.release();
}

void DataSharingSDKDelegateAndroid::AddAccessToken(
    const data_sharing_pb::AddAccessTokenParams& params,
    AddAccessTokenCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  std::string add_access_token_params;
  params.SerializeToString(&add_access_token_params);
  std::unique_ptr<AddAccessTokenCallback> wrapped_callback =
      std::make_unique<AddAccessTokenCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  Java_DataSharingSDKDelegateBridge_addAccessToken(
      env, java_obj_, ConvertUTF8ToJavaString(env, add_access_token_params),
      j_native_ptr);
  // We expect Java to always call us back through
  // JNI_DataSharingSDKDelegateBridge_RunAddAccessTokenCallback.
  wrapped_callback.release();
}

static void JNI_DataSharingSDKDelegateBridge_RunCreateGroupCallback(
    JNIEnv* env,
    jlong callback,
    const jni_zero::JavaParamRef<jbyteArray>& j_serlialized_proto,
    jint j_status) {
  std::unique_ptr<DataSharingSDKDelegateAndroid::CreateGroupCallback>
      callback_ptr(
          reinterpret_cast<DataSharingSDKDelegateAndroid::CreateGroupCallback*>(
              callback));
  std::string str;
  base::android::JavaByteArrayToString(env, j_serlialized_proto, &str);
  data_sharing_pb::CreateGroupResult result;
  if (j_status != 0 || str.empty() || !result.ParseFromString(str)) {
    std::move(*callback_ptr).Run(base::unexpected(absl::CancelledError()));
  } else {
    std::move(*callback_ptr).Run(std::move(result));
  }
}

static void JNI_DataSharingSDKDelegateBridge_RunReadGroupsCallback(
    JNIEnv* env,
    jlong callback,
    const jni_zero::JavaParamRef<jbyteArray>& j_serlialized_proto,
    jint j_status) {
  std::unique_ptr<DataSharingSDKDelegateAndroid::ReadGroupsCallback>
      callback_ptr(
          reinterpret_cast<DataSharingSDKDelegateAndroid::ReadGroupsCallback*>(
              callback));
  std::string str;
  base::android::JavaByteArrayToString(env, j_serlialized_proto, &str);
  data_sharing_pb::ReadGroupsResult result;
  if (j_status != 0 || str.empty() || !result.ParseFromString(str)) {
    std::move(*callback_ptr).Run(base::unexpected(absl::CancelledError()));
  } else {
    std::move(*callback_ptr).Run(std::move(result));
  }
}

static void JNI_DataSharingSDKDelegateBridge_RunGetStatusCallback(
    JNIEnv* env,
    jlong callback,
    jint j_status) {
  std::unique_ptr<DataSharingSDKDelegateAndroid::GetStatusCallback>
      callback_ptr(
          reinterpret_cast<DataSharingSDKDelegateAndroid::GetStatusCallback*>(
              callback));
  absl::Status error_status =
      (j_status == 0) ? absl::OkStatus() : absl::CancelledError();
  std::move(*callback_ptr).Run(error_status);
}

static void JNI_DataSharingSDKDelegateBridge_RunLookupGaiaIdByEmailCallback(
    JNIEnv* env,
    jlong callback,
    const jni_zero::JavaParamRef<jbyteArray>& j_serlialized_proto,
    jint j_status) {
  std::unique_ptr<DataSharingSDKDelegateAndroid::LookupGaiaIdByEmailCallback>
      callback_ptr(reinterpret_cast<
                   DataSharingSDKDelegateAndroid::LookupGaiaIdByEmailCallback*>(
          callback));
  std::string str;
  base::android::JavaByteArrayToString(env, j_serlialized_proto, &str);
  data_sharing_pb::LookupGaiaIdByEmailResult result;
  if (j_status != 0 || str.empty() || !result.ParseFromString(str)) {
    std::move(*callback_ptr).Run(base::unexpected(absl::CancelledError()));
  } else {
    std::move(*callback_ptr).Run(std::move(result));
  }
}

static void JNI_DataSharingSDKDelegateBridge_RunAddAccessTokenCallback(
    JNIEnv* env,
    jlong callback,
    const jni_zero::JavaParamRef<jbyteArray>& j_serlialized_proto,
    jint j_status) {
  std::unique_ptr<DataSharingSDKDelegateAndroid::AddAccessTokenCallback>
      callback_ptr(reinterpret_cast<
                   DataSharingSDKDelegateAndroid::AddAccessTokenCallback*>(
          callback));
  std::string str;
  base::android::JavaByteArrayToString(env, j_serlialized_proto, &str);
  data_sharing_pb::AddAccessTokenResult result;
  if (j_status != 0 || str.empty() || !result.ParseFromString(str)) {
    std::move(*callback_ptr).Run(base::unexpected(absl::CancelledError()));
  } else {
    std::move(*callback_ptr).Run(std::move(result));
  }
}

}  // namespace data_sharing
