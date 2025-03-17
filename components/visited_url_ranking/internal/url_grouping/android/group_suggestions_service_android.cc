// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/android/group_suggestions_service_android.h"

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/visited_url_ranking/internal/jni_headers/GroupSuggestionsServiceImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace visited_url_ranking {
namespace {

const char kGroupSuggestionsServiceBridgeKey[] =
    "group_suggestions_service_bridge";

}  // namespace

// This function is declared in group_suggestions_service.h and
// should be linked in to any binary using
// GroupSuggestionsService::GetJavaObject.
// static
ScopedJavaLocalRef<jobject> GroupSuggestionsService::GetJavaObject(
    GroupSuggestionsService* service) {
  if (!service->GetUserData(kGroupSuggestionsServiceBridgeKey)) {
    service->SetUserData(
        kGroupSuggestionsServiceBridgeKey,
        std::make_unique<GroupSuggestionsServiceAndroid>(service));
  }

  GroupSuggestionsServiceAndroid* bridge =
      static_cast<GroupSuggestionsServiceAndroid*>(
          service->GetUserData(kGroupSuggestionsServiceBridgeKey));

  return bridge->GetJavaObject();
}

GroupSuggestionsServiceAndroid::GroupSuggestionsServiceAndroid(
    GroupSuggestionsService* group_suggestions_service)
    : group_suggestions_service_(group_suggestions_service) {
  DCHECK(group_suggestions_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_GroupSuggestionsServiceImpl_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

GroupSuggestionsServiceAndroid::~GroupSuggestionsServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GroupSuggestionsServiceImpl_clearNativePtr(env, java_obj_);
}

void GroupSuggestionsServiceAndroid::DidAddTab(JNIEnv* env,
                                               int tab_id,
                                               int type) {
  group_suggestions_service_->GetTabEventTracker()->DidAddTab(tab_id);
}

ScopedJavaLocalRef<jobject> GroupSuggestionsServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace visited_url_ranking
