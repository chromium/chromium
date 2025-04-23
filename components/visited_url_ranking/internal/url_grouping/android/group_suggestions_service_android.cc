// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/android/group_suggestions_service_android.h"

#include <memory>

#include "base/android/jni_array.h"
#include "base/android/jni_callback.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/tab_event_tracker.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/visited_url_ranking/internal/jni_headers/DelegateBridge_jni.h"
#include "components/visited_url_ranking/internal/jni_headers/GroupSuggestionsServiceImpl_jni.h"
#include "components/visited_url_ranking/public/jni_headers/GroupSuggestion_jni.h"
#include "components/visited_url_ranking/public/jni_headers/GroupSuggestions_jni.h"
#include "components/visited_url_ranking/public/jni_headers/UserResponseMetadata_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace visited_url_ranking {
namespace {

const char kGroupSuggestionsServiceBridgeKey[] =
    "group_suggestions_service_bridge";

// TODO(crbug.com/397221723): Rethink the conversion plan. Maybe update the Java
// API to do the conversion at call site.
TabEventTracker::TabSelectionType ConvertIntToTabSelectionType(
    int tab_selection_type) {
  switch (tab_selection_type) {
    case 0:
      return TabEventTracker::TabSelectionType::kFromCloseActiveTab;
    case 1:
      return TabEventTracker::TabSelectionType::kFromAppExit;
    case 2:
      return TabEventTracker::TabSelectionType::kFromNewTab;
    case 3:
      return TabEventTracker::TabSelectionType::kFromUser;
    case 4:
      return TabEventTracker::TabSelectionType::kFromOmnibox;
    case 5:
      return TabEventTracker::TabSelectionType::kFromUndoClosure;
    default:
      return TabEventTracker::TabSelectionType::kUnknown;
  }
}

GroupSuggestionsDelegate::UserResponse ConvertIntToUserResponse(
    int user_response) {
  switch (user_response) {
    case 0:
      return GroupSuggestionsDelegate::UserResponse::kUnknown;
    case 1:
      return GroupSuggestionsDelegate::UserResponse::kNotShown;
    case 2:
      return GroupSuggestionsDelegate::UserResponse::kAccepted;
    case 3:
      return GroupSuggestionsDelegate::UserResponse::kRejected;
    case 4:
      return GroupSuggestionsDelegate::UserResponse::kIgnored;
    default:
      return GroupSuggestionsDelegate::UserResponse::kUnknown;
  }
}
}  // namespace

// static
GroupSuggestionsDelegate::UserResponseMetadata
GroupSuggestionsServiceAndroid::ToNativeUserResponse(
    JNIEnv* env,
    const JavaRef<jobject>& j_metadata) {
  GroupSuggestionsDelegate::UserResponseMetadata data;
  int suggestion_id_int = static_cast<int>(
      Java_UserResponseMetadata_getSuggestionId(env, j_metadata));
  data.suggestion_id =
      UrlGroupingSuggestionId::FromUnsafeValue(suggestion_id_int);
  int response_int = static_cast<int>(
      Java_UserResponseMetadata_getUserResponse(env, j_metadata));
  data.user_response = ConvertIntToUserResponse(response_int);
  return data;
}

ScopedJavaLocalRef<jobject>
GroupSuggestionsServiceAndroid::FromNativeUserResponse(
    JNIEnv* env,
    const GroupSuggestionsDelegate::UserResponseMetadata& metadata) {
  return Java_UserResponseMetadata_create(
      env, static_cast<jint>(metadata.suggestion_id.GetUnsafeValue()),
      static_cast<jint>(metadata.user_response));
}

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
  ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<GroupSuggestionsService> group_suggestions_service_;
};

GroupSuggestionsServiceAndroid::SuggestionDelegateBridge::
    SuggestionDelegateBridge(
        GroupSuggestionsService* group_suggestions_service,
        GroupSuggestionsServiceAndroid* group_suggestions_service_android)
    : group_suggestions_service_(group_suggestions_service) {
  java_obj_ = ScopedJavaGlobalRef<jobject>(
      AttachCurrentThread(),
      group_suggestions_service_android->GetJavaDelegateBridge());
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
  std::vector<base::android::ScopedJavaLocalRef<jobject>> suggestions;
  suggestions.reserve(group_suggestions.suggestions.size());
  for (const auto& group_suggestion : group_suggestions.suggestions) {
    ScopedJavaLocalRef<jintArray> tab_ids =
        base::android::ToJavaIntArray(env, group_suggestion.tab_ids);
    ScopedJavaLocalRef<jstring> suggested_name =
        base::android::ConvertUTF16ToJavaString(
            env, group_suggestion.suggested_name);
    ScopedJavaLocalRef<jstring> promo_header =
        base::android::ConvertUTF8ToJavaString(env,
                                               group_suggestion.promo_header);
    ScopedJavaLocalRef<jstring> promo_contents =
        base::android::ConvertUTF8ToJavaString(env,
                                               group_suggestion.promo_contents);
    suggestions.emplace_back(Java_GroupSuggestion_createGroupSuggestion(
        env, tab_ids, group_suggestion.suggestion_id.GetUnsafeValue(),
        static_cast<int>(group_suggestion.suggestion_reason), suggested_name,
        promo_header, promo_contents));
  }
  Java_DelegateBridge_showSuggestion(
      env, java_obj_,
      Java_GroupSuggestions_createGroupSuggestions(env, suggestions),
      base::android::ToJniCallback(env, std::move(response_callback)));
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
                                               int tab_launch_type) {
  group_suggestions_service_->GetTabEventTracker()->DidAddTab(tab_id,
                                                              tab_launch_type);
}

void GroupSuggestionsServiceAndroid::DidSelectTab(
    JNIEnv* env,
    int tab_id,
    const JavaParamRef<jobject>& url,
    int tab_selection_type,
    int last_id) {
  group_suggestions_service_->GetTabEventTracker()->DidSelectTab(
      tab_id, url::GURLAndroid::ToNativeGURL(env, url),
      ConvertIntToTabSelectionType(tab_selection_type), last_id);
}

void GroupSuggestionsServiceAndroid::WillCloseTab(JNIEnv* env, int tab_id) {
  group_suggestions_service_->GetTabEventTracker()->WillCloseTab(tab_id);
}

void GroupSuggestionsServiceAndroid::TabClosureUndone(JNIEnv* env, int tab_id) {
  group_suggestions_service_->GetTabEventTracker()->TabClosureUndone(tab_id);
}

void GroupSuggestionsServiceAndroid::TabClosureCommitted(JNIEnv* env,
                                                         int tab_id) {
  group_suggestions_service_->GetTabEventTracker()->TabClosureCommitted(tab_id);
}

void GroupSuggestionsServiceAndroid::OnDidFinishNavigation(
    JNIEnv* env,
    int tab_id,
    int page_transition) {
  group_suggestions_service_->GetTabEventTracker()->OnDidFinishNavigation(
      tab_id, ui::PageTransitionFromInt(page_transition));
}

void GroupSuggestionsServiceAndroid::DidEnterTabSwitcher(JNIEnv* env) {
  group_suggestions_service_->GetTabEventTracker()->DidEnterTabSwitcher();
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
