// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/comments/comments_service_bridge.h"

#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "base/uuid.h"
#include "components/collaboration/public/comments/comments_service.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/comments_jni_headers/CommentsServiceBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::RunBooleanCallbackAndroid;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using data_sharing::GroupId;

namespace collaboration::comments::android {

namespace {
const char kCommentsServiceBridgeUserDataKey[] = "comments_service";
}  // namespace

// static
ScopedJavaLocalRef<jobject> CommentsServiceBridge::GetBridgeForCommentsService(
    CommentsService* service) {
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
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

CommentsServiceBridge::~CommentsServiceBridge() {
  Java_CommentsServiceBridge_onNativeDestroyed(AttachCurrentThread(),
                                               java_ref_);
}

ScopedJavaLocalRef<jobject> CommentsServiceBridge::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

bool CommentsServiceBridge::IsInitialized(JNIEnv* env) {
  return service_->IsInitialized();
}

bool CommentsServiceBridge::IsEmptyService(JNIEnv* env) {
  return service_->IsEmptyService();
}

base::Uuid CommentsServiceBridge::AddComment(
    JNIEnv* env,
    const std::string& collaboration_id,
    const GURL& url,
    const std::string& content,
    const std::optional<base::Uuid>& parent_comment_id,
    const JavaRef<jobject>& j_success_callback) {
  auto callback =
      base::BindOnce(&RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_success_callback));
  return service_->AddComment(GroupId(collaboration_id), url, content,
                              parent_comment_id, std::move(callback));
}

void CommentsServiceBridge::EditComment(
    JNIEnv* env,
    const base::Uuid& comment_id,
    const std::string& new_content,
    const JavaRef<jobject>& j_success_callback) {
  auto callback =
      base::BindOnce(&RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_success_callback));
  service_->EditComment(comment_id, new_content, std::move(callback));
}

void CommentsServiceBridge::DeleteComment(
    JNIEnv* env,
    const base::Uuid& comment_id,
    const JavaRef<jobject>& j_success_callback) {
  auto callback =
      base::BindOnce(&RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_success_callback));
  service_->DeleteComment(comment_id, std::move(callback));
}

void CommentsServiceBridge::QueryComments(
    JNIEnv* env,
    const JavaRef<jobject>& j_filter_criteria,
    const JavaRef<jobject>& j_pagination_criteria,
    const JavaRef<jobject>& j_callback) {
  // TODO(crbug.com/435005417): Implement this.
}

void CommentsServiceBridge::AddObserver(
    JNIEnv* env,
    const JavaRef<jobject>& j_observer,
    const JavaRef<jobject>& j_filter_criteria) {
  // TODO(crbug.com/435005417): Implement this.
}

void CommentsServiceBridge::RemoveObserver(JNIEnv* env,
                                           const JavaRef<jobject>& j_observer) {
  // TODO(crbug.com/435005417): Implement this.
}

}  // namespace collaboration::comments::android

DEFINE_JNI(CommentsServiceBridge)
