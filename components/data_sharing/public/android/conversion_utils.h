// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_ANDROID_CONVERSION_UTILS_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_ANDROID_CONVERSION_UTILS_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/service_status.h"

using base::android::ScopedJavaLocalRef;

namespace data_sharing::conversion {

// Creates an object of org.chromium.components.data_sharing.ServiceStatus.
ScopedJavaLocalRef<jobject> CreateJavaServiceStatus(
    JNIEnv* env,
    const ServiceStatus& status);

// Creates an object of org.chromium.components.data_sharing.GroupMember.
ScopedJavaLocalRef<jobject> CreateJavaGroupMember(JNIEnv* env,
                                                  const GroupMember& member);

// Creates an object of org.chromium.components.data_sharing.GroupToken.
ScopedJavaLocalRef<jobject> CreateJavaGroupToken(JNIEnv* env,
                                                 const GroupToken& token);

// Creates an object of org.chromium.components.data_sharing.GroupData.
ScopedJavaLocalRef<jobject> CreateJavaGroupData(JNIEnv* env,
                                                const GroupData& group_data);

ScopedJavaLocalRef<jobjectArray> CreateGroupedDataArray(
    JNIEnv* env,
    const std::set<GroupData>& data);

// Creates an object of org.chromium.components.data_sharing.SharedEntity.
ScopedJavaLocalRef<jobject> CreateJavaSharedEntity(JNIEnv* env,
                                                   const SharedEntity& entity);

ScopedJavaLocalRef<jobjectArray> CreateJavaSharedEntityArray(
    JNIEnv* env,
    const std::vector<SharedEntity>& entities);

ScopedJavaLocalRef<jobject> CreateDataSharingNetworkResult(
    JNIEnv* env,
    DataSharingNetworkLoader::LoadResult* response);

}  // namespace data_sharing::conversion

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_ANDROID_CONVERSION_UTILS_H_
