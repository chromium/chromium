// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_conversion_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/data_sharing/public/android/conversion_utils.h"
#include "components/data_sharing/public/group_data.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/data_sharing/internal/jni_headers/DataSharingConversionBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToTypedJavaArrayOfObjects;

namespace data_sharing {

ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreateGroupDataOrFailureOutcome(
    JNIEnv* env,
    const DataSharingService::GroupDataOrFailureOutcome& data) {
  ScopedJavaLocalRef<jobject> j_group_data;
  DataSharingService::PeopleGroupActionFailure failure =
      DataSharingService::PeopleGroupActionFailure::kUnknown;
  if (data.has_value()) {
    j_group_data = conversion::CreateJavaGroupData(env, data.value());
  } else {
    failure = data.error();
  }
  return Java_DataSharingConversionBridge_createGroupDataOrFailureOutcome(
      env, j_group_data, static_cast<int>(failure));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreateGroupDataSetOrFailureOutcome(
    JNIEnv* env,
    const DataSharingService::GroupsDataSetOrFailureOutcome& data) {
  std::vector<ScopedJavaLocalRef<jobject>> j_groups_data;
  DataSharingService::PeopleGroupActionFailure failure =
      DataSharingService::PeopleGroupActionFailure::kUnknown;

  ScopedJavaLocalRef<jobjectArray> j_group_array;
  if (data.has_value()) {
    j_group_array = conversion::CreateGroupedDataArray(env, data.value());
  } else {
    failure = data.error();
  }
  return Java_DataSharingConversionBridge_createGroupDataSetOrFailureOutcome(
      env, j_group_array, static_cast<int>(failure));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreatePeopleGroupActionOutcome(JNIEnv* env,
                                                            int value) {
  return Java_DataSharingConversionBridge_createPeopleGroupActionOutcome(
      AttachCurrentThread(), value);
}

// static
ScopedJavaLocalRef<jobject> DataSharingConversionBridge::CreateParseURLResult(
    JNIEnv* env,
    const DataSharingService::ParseURLResult& data) {
  ScopedJavaLocalRef<jobject> j_group_data;
  DataSharingService::ParseURLStatus status =
      DataSharingService::ParseURLStatus::kUnknown;
  if (data.has_value()) {
    j_group_data = conversion::CreateJavaGroupToken(env, data.value());
    status = DataSharingService::ParseURLStatus::kSuccess;
  } else {
    status = data.error();
  }
  return Java_DataSharingConversionBridge_createParseURLResult(
      env, j_group_data, static_cast<int>(status));
}

// static
ScopedJavaLocalRef<jobject>
DataSharingConversionBridge::CreateSharedDataPreviewOrFailureOutcome(
    JNIEnv* env,
    const DataSharingService::SharedDataPreviewOrFailureOutcome& data) {
  std::vector<ScopedJavaLocalRef<jobject>> j_entities;
  DataSharingService::PeopleGroupActionFailure failure =
      DataSharingService::PeopleGroupActionFailure::kUnknown;

  ScopedJavaLocalRef<jobjectArray> j_entities_array;
  if (data.has_value()) {
    j_entities_array = conversion::CreateJavaSharedEntityArray(
        env, data.value().shared_entities);

  } else {
    failure = data.error();
  }

  return Java_DataSharingConversionBridge_createSharedDataPreviewOrFailureOutcome(
      env, j_entities_array, static_cast<int>(failure));
}
}  // namespace data_sharing
