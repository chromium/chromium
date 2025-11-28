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
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/comments_jni_headers/CommentsServiceBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
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

bool CommentsServiceBridge::IsInitialized(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller) {
  return service_->IsInitialized();
}

bool CommentsServiceBridge::IsEmptyService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller) {
  return service_->IsEmptyService();
}

ScopedJavaLocalRef<jstring> CommentsServiceBridge::AddComment(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_collaboration_id,
    const JavaParamRef<jobject>& j_url,
    const JavaParamRef<jstring>& j_content,
    const JavaParamRef<jstring>& j_parent_comment_id,
    const JavaParamRef<jobject>& j_success_callback) {
  std::optional<base::Uuid> parent_comment_id;
  if (j_parent_comment_id) {
    parent_comment_id = tab_groups::JavaStringToUuid(env, j_parent_comment_id);
  }

  auto callback =
      base::BindOnce(&RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_success_callback));
  base::Uuid new_comment_id = service_->AddComment(
      GroupId(ConvertJavaStringToUTF8(env, j_collaboration_id)),
      url::GURLAndroid::ToNativeGURL(env, j_url),
      ConvertJavaStringToUTF8(env, j_content), parent_comment_id,
      std::move(callback));

  return tab_groups::UuidToJavaString(env, new_comment_id);
}

void CommentsServiceBridge::EditComment(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_comment_id,
    const JavaParamRef<jstring>& j_new_content,
    const JavaParamRef<jobject>& j_success_callback) {
  auto callback =
      base::BindOnce(&RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_success_callback));
  service_->EditComment(tab_groups::JavaStringToUuid(env, j_comment_id),
                        ConvertJavaStringToUTF8(env, j_new_content),
                        std::move(callback));
}

void CommentsServiceBridge::DeleteComment(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_comment_id,
    const JavaParamRef<jobject>& j_success_callback) {
  auto callback =
      base::BindOnce(&RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_success_callback));
  service_->DeleteComment(tab_groups::JavaStringToUuid(env, j_comment_id),
                          std::move(callback));
}

void CommentsServiceBridge::QueryComments(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_filter_criteria,
    const JavaParamRef<jobject>& j_pagination_criteria,
    const JavaParamRef<jobject>& j_callback) {
  // TODO(crbug.com/435005417): Implement this.
}

void CommentsServiceBridge::AddObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_observer,
    const JavaParamRef<jobject>& j_filter_criteria) {
  // TODO(crbug.com/435005417): Implement this.
}

void CommentsServiceBridge::RemoveObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_observer) {
  // TODO(crbug.com/435005417): Implement this.
}

}  // namespace collaboration::comments::android

DEFINE_JNI(CommentsServiceBridge)
