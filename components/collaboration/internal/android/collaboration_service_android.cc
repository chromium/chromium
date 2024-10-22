// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/collaboration_service_android.h"

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/collaboration/internal/jni_headers/CollaborationServiceImpl_jni.h"
#include "components/collaboration/public/collaboration_service.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

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

ScopedJavaLocalRef<jobject> CollaborationServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace collaboration
