// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/collaboration_service_android.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/collaboration/internal/core_jni_headers/CollaborationServiceImpl_jni.h"
#include "components/collaboration/public/collaboration_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/public/core_jni_headers/ServiceStatus_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using data_sharing::GroupData;
using data_sharing::GroupId;

namespace collaboration {
namespace {

const char kCollaborationServiceBridgeKey[] = "collaboration_service_bridge";

}  // namespace

// This function is declared in collaboration_service.h and
// should be linked in to any binary using
// CollaborationService::GetJavaObject.
// static
ScopedJavaLocalRef<jobject> CollaborationService::GetJavaObject(
    CollaborationService* service) {
  if (!service->GetUserData(kCollaborationServiceBridgeKey)) {
    service->SetUserData(
        kCollaborationServiceBridgeKey,
        std::make_unique<CollaborationServiceAndroid>(service));
  }

  CollaborationServiceAndroid* bridge =
      static_cast<CollaborationServiceAndroid*>(
          service->GetUserData(kCollaborationServiceBridgeKey));

  return bridge->GetJavaObject();
}

CollaborationServiceAndroid::CollaborationServiceAndroid(
    CollaborationService* collaboration_service)
    : collaboration_service_(collaboration_service) {
  DCHECK(collaboration_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_CollaborationServiceImpl_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

CollaborationServiceAndroid::~CollaborationServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationServiceImpl_clearNativePtr(env, java_obj_);
}

bool CollaborationServiceAndroid::IsEmptyService(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return collaboration_service_->IsEmptyService();
}

ScopedJavaLocalRef<jobject> CollaborationServiceAndroid::GetServiceStatus(
    JNIEnv* env) {
  ServiceStatus status = collaboration_service_->GetServiceStatus();

  return Java_ServiceStatus_createServiceStatus(
      env, static_cast<int>(status.signin_status),
      static_cast<int>(status.sync_status),
      static_cast<int>(status.collaboration_status));
}

jint CollaborationServiceAndroid::GetCurrentUserRoleForGroup(
    JNIEnv* env,
    const JavaParamRef<jstring>& group_id) {
  data_sharing::MemberRole role =
      collaboration_service_->GetCurrentUserRoleForGroup(
          GroupId(ConvertJavaStringToUTF8(env, group_id)));

  return static_cast<jint>(role);
}

ScopedJavaLocalRef<jobject> CollaborationServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace collaboration
