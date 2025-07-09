// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/comments/comments_service_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "components/collaboration/public/comments/comments_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/comments_jni_headers/CommentsServiceBridge_jni.h"

namespace collaboration::comments::android {

namespace {
const char kCommentsServiceBridgeUserDataKey[] = "comments_service";
}  // namespace

// static
base::android::ScopedJavaLocalRef<jobject>
CommentsServiceBridge::GetBridgeForCommentsService(CommentsService* service) {
  if (!service->GetUserData(kCommentsServiceBridgeUserDataKey)) {
    service->SetUserData(kCommentsServiceBridgeUserDataKey,
                         base::WrapUnique(new CommentsServiceBridge(service)));
  }

  CommentsServiceBridge* bridge = static_cast<CommentsServiceBridge*>(
      service->GetUserData(kCommentsServiceBridgeUserDataKey));
  return bridge->GetJavaObject();
}

// static
std::unique_ptr<CommentsServiceBridge> CommentsServiceBridge::CreateForTest(
    CommentsService* service) {
  CommentsServiceBridge* bridge = new CommentsServiceBridge(service);
  return base::WrapUnique(bridge);
}

CommentsServiceBridge::CommentsServiceBridge(CommentsService* service)
    : service_(service) {
  java_ref_.Reset(Java_CommentsServiceBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

CommentsServiceBridge::~CommentsServiceBridge() {
  Java_CommentsServiceBridge_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_ref_);
}

base::android::ScopedJavaLocalRef<jobject>
CommentsServiceBridge::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

bool CommentsServiceBridge::IsInitialized(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller) {
  return service_->IsInitialized();
}

bool CommentsServiceBridge::IsEmptyService(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller) {
  return service_->IsEmptyService();
}

}  // namespace collaboration::comments::android
