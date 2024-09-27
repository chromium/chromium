// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/android/conversion_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/service_status.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/data_sharing/public/jni_headers/DataSharingNetworkResult_jni.h"
#include "components/data_sharing/public/jni_headers/GroupData_jni.h"
#include "components/data_sharing/public/jni_headers/GroupMember_jni.h"
#include "components/data_sharing/public/jni_headers/GroupToken_jni.h"
#include "components/data_sharing/public/jni_headers/ServiceStatus_jni.h"
#include "components/data_sharing/public/jni_headers/SharedEntity_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using base::android::ToTypedJavaArrayOfObjects;

namespace data_sharing::conversion {

ScopedJavaLocalRef<jobject> CreateJavaServiceStatus(
    JNIEnv* env,
    const ServiceStatus& status) {
  return Java_ServiceStatus_createServiceStatus(
      env, static_cast<int>(status.signin_status),
      static_cast<int>(status.sync_status),
      static_cast<int>(status.collaboration_status));
}

ScopedJavaLocalRef<jobject> CreateJavaGroupMember(JNIEnv* env,
                                                  const GroupMember& member) {
  return Java_GroupMember_createGroupMember(
      env, ConvertUTF8ToJavaString(env, member.gaia_id),
      ConvertUTF8ToJavaString(env, member.display_name),
      ConvertUTF8ToJavaString(env, member.email), static_cast<int>(member.role),
      url::GURLAndroid::FromNativeGURL(env, member.avatar_url),
      ConvertUTF8ToJavaString(env, member.given_name));
}

ScopedJavaLocalRef<jobject> CreateJavaGroupToken(JNIEnv* env,
                                                 const GroupToken& token) {
  return Java_GroupToken_createGroupToken(
      env, ConvertUTF8ToJavaString(env, token.group_id.value()),
      ConvertUTF8ToJavaString(env, token.access_token));
}

ScopedJavaLocalRef<jobject> CreateJavaGroupData(JNIEnv* env,
                                                const GroupData& group_data) {
  std::vector<ScopedJavaLocalRef<jobject>> j_members;
  j_members.reserve(group_data.members.size());
  for (const GroupMember& member : group_data.members) {
    j_members.push_back(CreateJavaGroupMember(env, member));
  }
  return Java_GroupData_createGroupData(
      env,
      ConvertUTF8ToJavaString(env, group_data.group_token.group_id.value()),
      ConvertUTF8ToJavaString(env, group_data.display_name),
      ToTypedJavaArrayOfObjects(
          env, base::make_span(j_members),
          org_chromium_components_data_1sharing_GroupMember_clazz(env)),
      ConvertUTF8ToJavaString(env, group_data.group_token.access_token));
}

ScopedJavaLocalRef<jobjectArray> CreateGroupedDataArray(
    JNIEnv* env,
    const std::set<GroupData>& groups) {
  std::vector<ScopedJavaLocalRef<jobject>> j_groups_data;
  for (const GroupData& group : groups) {
    j_groups_data.push_back(CreateJavaGroupData(env, group));
  }

  ScopedJavaLocalRef<jobjectArray> j_group_array;
  if (!j_groups_data.empty()) {
    j_group_array = ToTypedJavaArrayOfObjects(
        env, base::make_span(j_groups_data),
        org_chromium_components_data_1sharing_GroupData_clazz(env));
  }

  return j_group_array;
}

ScopedJavaLocalRef<jobject> CreateJavaSharedEntity(JNIEnv* env,
                                                   const SharedEntity& entity) {
  int size = entity.specifics.ByteSize();
  std::vector<uint8_t> data(size);
  entity.specifics.SerializeToArray(data.data(), size);
  return Java_SharedEntity_createSharedEntity(
      env, ConvertUTF8ToJavaString(env, entity.group_id.value()),
      ConvertUTF8ToJavaString(env, entity.name), entity.version,
      entity.update_time.InMillisecondsSinceUnixEpoch(),
      entity.create_time.InMillisecondsSinceUnixEpoch(),
      ConvertUTF8ToJavaString(env, entity.client_tag_hash),
      base::android::ToJavaByteArray(env, data));
}

ScopedJavaLocalRef<jobjectArray> CreateJavaSharedEntityArray(
    JNIEnv* env,
    const std::vector<SharedEntity>& entities) {
  std::vector<ScopedJavaLocalRef<jobject>> j_entities;
  for (const SharedEntity& entity : entities) {
    j_entities.push_back(CreateJavaSharedEntity(env, entity));
  }

  ScopedJavaLocalRef<jobjectArray> j_entities_array;
  if (!j_entities.empty()) {
    j_entities_array = ToTypedJavaArrayOfObjects(
        env, base::make_span(j_entities),
        org_chromium_components_data_1sharing_SharedEntity_clazz(env));
  }
  return j_entities_array;
}

ScopedJavaLocalRef<jobject> CreateDataSharingNetworkResult(
    JNIEnv* env,
    DataSharingNetworkLoader::LoadResult* response) {
  if (response == nullptr) {
    return ScopedJavaLocalRef<jobject>();
  }

  return Java_DataSharingNetworkResult_createDataSharingNetworkResult(
      env,
      ToJavaByteArray(env, std::vector<uint8_t>(response->result_bytes.begin(),
                                                response->result_bytes.end())),
      static_cast<int>(response->status));
}

}  // namespace data_sharing::conversion
