// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/android/group_suggestions_service_android.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/visited_url_ranking/internal/jni_headers/DelegateBridge_jni.h"
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

// Native counterpart of Java DelegateBridge. Observes the native service and
// sends events to the Java bridge.
class GroupSuggestionsServiceAndroid::SuggestionDelegateBridge
    : public GroupSuggestionsDelegate {
 public:
  SuggestionDelegateBridge(
      GroupSuggestionsService* group_suggestions_service,
      GroupSuggestionsServiceAndroid* group_suggestions_service_android);
  ~SuggestionDelegateBridge() override;

  SuggestionDelegateBridge(const SuggestionDelegateBridge&) = delete;
  SuggestionDelegateBridge& operator=(const SuggestionDelegateBridge&) = delete;

  // GroupSuggestionsDelegate impl:
  void ShowSuggestion(const GroupSuggestions& group_suggestions,
                      SuggestionResponseCallback response_callback) override;
  void OnDumpStateForFeedback(const std::string& dump_state) override;

 private:
  ScopedJavaLocalRef<jobject> java_obj_;
  raw_ptr<GroupSuggestionsService> group_suggestions_service_;
};

GroupSuggestionsServiceAndroid::SuggestionDelegateBridge::
    SuggestionDelegateBridge(
        GroupSuggestionsService* group_suggestions_service,
        GroupSuggestionsServiceAndroid* group_suggestions_service_android)
    : group_suggestions_service_(group_suggestions_service) {
  java_obj_ = group_suggestions_service_android->GetJavaDelegateBridge();
  // TODO(crbug.com/397221723): Specify correct window information in scope to
  // handle multi-window.
  group_suggestions_service->RegisterDelegate(this,
                                              GroupSuggestionsService::Scope());
}

GroupSuggestionsServiceAndroid::SuggestionDelegateBridge::
    ~SuggestionDelegateBridge() {
  group_suggestions_service_->UnregisterDelegate(this);
}

void GroupSuggestionsServiceAndroid::SuggestionDelegateBridge::ShowSuggestion(
    const GroupSuggestions& group_suggestions,
    GroupSuggestionsDelegate::SuggestionResponseCallback response_callback) {
  JNIEnv* env = AttachCurrentThread();
  Java_DelegateBridge_showSuggestion(env, java_obj_);
}

void GroupSuggestionsServiceAndroid::SuggestionDelegateBridge::
    OnDumpStateForFeedback(const std::string& dump_state) {
  JNIEnv* env = AttachCurrentThread();
  Java_DelegateBridge_onDumpStateForFeedback(
      env, java_obj_, base::android::ConvertUTF8ToJavaString(env, dump_state));
}

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

  delegate_bridge_ = std::make_unique<SuggestionDelegateBridge>(
      group_suggestions_service, this);
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

ScopedJavaLocalRef<jobject>
GroupSuggestionsServiceAndroid::GetJavaDelegateBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_GroupSuggestionsServiceImpl_getDelegateBridge(env,
                                                            GetJavaObject());
}

}  // namespace visited_url_ranking
