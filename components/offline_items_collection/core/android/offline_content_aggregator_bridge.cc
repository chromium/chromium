// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/android/offline_content_aggregator_bridge.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/offline_items_collection/core/android/offline_item_bridge.h"
#include "components/offline_items_collection/core/android/offline_item_share_info_bridge.h"
#include "components/offline_items_collection/core/android/offline_item_visuals_bridge.h"
#include "components/offline_items_collection/core/jni_headers/OfflineContentAggregatorBridge_jni.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/throttled_offline_content_provider.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_items_collection {

using GetVisualsOptions = OfflineContentProvider::GetVisualsOptions;

namespace android {

namespace {
const char kOfflineContentAggregatorBridgeUserDataKey[] = "aggregator_bridge";

ContentId JNI_OfflineContentAggregatorBridge_CreateContentId(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id) {
  return ContentId(ConvertJavaStringToUTF8(env, j_namespace),
                   ConvertJavaStringToUTF8(env, j_id));
}

// Helper callback that glues the Java-specific callback logic to the native
// VisualsCallback that is required by the OfflineContentProvider native class.
void GetVisualsForItemHelperCallback(
    ScopedJavaGlobalRef<jobject> j_callback,
    const ContentId& id,
    std::unique_ptr<OfflineItemVisuals> visuals) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflineContentAggregatorBridge_onVisualsAvailable(
      env, j_callback, ConvertUTF8ToJavaString(env, id.name_space),
      ConvertUTF8ToJavaString(env, id.id),
      OfflineItemVisualsBridge::CreateOfflineItemVisuals(env,
                                                         std::move(visuals)));
}

void ForwardShareInfoToJavaCallback(
    ScopedJavaGlobalRef<jobject> j_callback,
    const ContentId& id,
    std::unique_ptr<OfflineItemShareInfo> shareInfo) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflineContentAggregatorBridge_onShareInfoAvailable(
      env, j_callback, ConvertUTF8ToJavaString(env, id.name_space),
      ConvertUTF8ToJavaString(env, id.id),
      OfflineItemShareInfoBridge::CreateOfflineItemShareInfo(
          env, std::move(shareInfo)));
}

void RenameItemCallback(ScopedJavaGlobalRef<jobject> j_callback,
                        RenameResult result) {
  base::android::RunIntCallbackAndroid(j_callback, static_cast<int>(result));
}

void RunGetAllItemsCallback(const base::android::JavaRef<jobject>& j_callback,
                            const std::vector<OfflineItem>& items) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback, OfflineItemBridge::CreateOfflineItemList(env, items));
}

void RunGetItemByIdCallback(const base::android::JavaRef<jobject>& j_callback,
                            const base::Optional<OfflineItem>& item) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback, item.has_value()
                      ? OfflineItemBridge::CreateOfflineItem(env, item.value())
                      : nullptr);
}

}  // namespace

// static
base::android::ScopedJavaLocalRef<jobject>
OfflineContentAggregatorBridge::GetBridgeForOfflineContentAggregator(
    OfflineContentAggregator* aggregator) {
  if (!aggregator->GetUserData(kOfflineContentAggregatorBridgeUserDataKey)) {
    aggregator->SetUserData(
        kOfflineContentAggregatorBridgeUserDataKey,
        base::WrapUnique(new OfflineContentAggregatorBridge(aggregator)));
  }
  OfflineContentAggregatorBridge* bridge =
      static_cast<OfflineContentAggregatorBridge*>(
          aggregator->GetUserData(kOfflineContentAggregatorBridgeUserDataKey));

  return ScopedJavaLocalRef<jobject>(bridge->java_ref_);
}

OfflineContentAggregatorBridge::OfflineContentAggregatorBridge(
    OfflineContentAggregator* aggregator)
    : provider_(std::make_unique<ThrottledOfflineContentProvider>(aggregator)) {
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(Java_OfflineContentAggregatorBridge_create(
      env, reinterpret_cast<intptr_t>(this)));

  provider_->AddObserver(this);
}

OfflineContentAggregatorBridge::~OfflineContentAggregatorBridge() {
  // TODO(dtrainor): Do not need to unregister because in the destructor of the
  // base class of OfflineContentAggregator.  Is |observers_| already dead?
  provider_->RemoveObserver(this);

  Java_OfflineContentAggregatorBridge_onNativeDestroyed(AttachCurrentThread(),
                                                        java_ref_);
}

void OfflineContentAggregatorBridge::OpenItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint launch_location,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id) {
  provider_->OpenItem(static_cast<LaunchLocation>(launch_location),
                      JNI_OfflineContentAggregatorBridge_CreateContentId(
                          env, j_namespace, j_id));
}

void OfflineContentAggregatorBridge::RemoveItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id) {
  provider_->RemoveItem(JNI_OfflineContentAggregatorBridge_CreateContentId(
      env, j_namespace, j_id));
}

void OfflineContentAggregatorBridge::CancelDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id) {
  provider_->CancelDownload(JNI_OfflineContentAggregatorBridge_CreateContentId(
      env, j_namespace, j_id));
}

void OfflineContentAggregatorBridge::PauseDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_guid) {
  provider_->PauseDownload(JNI_OfflineContentAggregatorBridge_CreateContentId(
      env, j_namespace, j_guid));
}

void OfflineContentAggregatorBridge::ResumeDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id,
    jboolean j_has_user_gesture) {
  provider_->ResumeDownload(JNI_OfflineContentAggregatorBridge_CreateContentId(
                                env, j_namespace, j_id),
                            j_has_user_gesture);
}

void OfflineContentAggregatorBridge::GetItemById(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id,
    const JavaParamRef<jobject>& jcallback) {
  OfflineContentProvider::SingleItemCallback callback =
      base::BindOnce(&RunGetItemByIdCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));
  provider_->GetItemById(JNI_OfflineContentAggregatorBridge_CreateContentId(
                             env, j_namespace, j_id),
                         std::move(callback));
}

void OfflineContentAggregatorBridge::GetAllItems(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcallback) {
  OfflineContentProvider::MultipleItemCallback callback =
      base::BindOnce(&RunGetAllItemsCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback));

  provider_->GetAllItems(std::move(callback));
}

void OfflineContentAggregatorBridge::GetVisualsForItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id,
    const JavaParamRef<jobject>& j_callback) {
  provider_->GetVisualsForItem(
      JNI_OfflineContentAggregatorBridge_CreateContentId(env, j_namespace,
                                                         j_id),
      GetVisualsOptions::IconOnly(),
      base::BindOnce(&GetVisualsForItemHelperCallback,
                     ScopedJavaGlobalRef<jobject>(env, j_callback)));
}

void OfflineContentAggregatorBridge::GetShareInfoForItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id,
    const JavaParamRef<jobject>& j_callback) {
  provider_->GetShareInfoForItem(
      JNI_OfflineContentAggregatorBridge_CreateContentId(env, j_namespace,
                                                         j_id),
      base::BindOnce(&ForwardShareInfoToJavaCallback,
                     ScopedJavaGlobalRef<jobject>(env, j_callback)));
}

void OfflineContentAggregatorBridge::RenameItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jstring>& j_namespace,
    const JavaParamRef<jstring>& j_id,
    const JavaParamRef<jstring>& j_name,
    const JavaParamRef<jobject>& j_callback) {
  base::OnceCallback<void(RenameResult)> callback = base::BindOnce(
      &RenameItemCallback,
      base::android::ScopedJavaGlobalRef<jobject>(env, j_callback));

  provider_->RenameItem(JNI_OfflineContentAggregatorBridge_CreateContentId(
                            env, j_namespace, j_id),
                        ConvertJavaStringToUTF8(env, j_name),
                        std::move(callback));
}

void OfflineContentAggregatorBridge::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  if (java_ref_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  Java_OfflineContentAggregatorBridge_onItemsAdded(
      env, java_ref_, OfflineItemBridge::CreateOfflineItemList(env, items));
}

void OfflineContentAggregatorBridge::OnItemRemoved(const ContentId& id) {
  if (java_ref_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  Java_OfflineContentAggregatorBridge_onItemRemoved(
      env, java_ref_, ConvertUTF8ToJavaString(env, id.name_space),
      ConvertUTF8ToJavaString(env, id.id));
}

void OfflineContentAggregatorBridge::OnItemUpdated(
    const OfflineItem& item,
    const base::Optional<UpdateDelta>& update_delta) {
  if (java_ref_.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  Java_OfflineContentAggregatorBridge_onItemUpdated(
      env, java_ref_, OfflineItemBridge::CreateOfflineItem(env, item),
      OfflineItemBridge::CreateUpdateDelta(env, update_delta));
}

}  // namespace android
}  // namespace offline_items_collection
