// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_ANDROID_GROUP_SUGGESTIONS_SERVICE_ANDROID_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_ANDROID_GROUP_SUGGESTIONS_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"

namespace visited_url_ranking {

// Helper class responsible for bridging the GroupSuggestionsService between
// C++ and Java.
class GroupSuggestionsServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit GroupSuggestionsServiceAndroid(GroupSuggestionsService* service);
  ~GroupSuggestionsServiceAndroid() override;

  static GroupSuggestionsDelegate::UserResponseMetadata ToNativeUserResponse(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_metadata);
  static jni_zero::ScopedJavaLocalRef<jobject> FromNativeUserResponse(
      JNIEnv* env,
      const GroupSuggestionsDelegate::UserResponseMetadata& metadata);

  void DidAddTab(JNIEnv* env, int tab_id, int tab_launch_type);

  void DidSelectTab(JNIEnv* env,
                    int tab_id,
                    const jni_zero::JavaParamRef<jobject>& url,
                    int tab_selection_type,
                    int last_id);

  void WillCloseTab(JNIEnv* env, int tab_id);

  void TabClosureUndone(JNIEnv* env, int tab_id);

  void TabClosureCommitted(JNIEnv* env, int tab_id);

  void OnDidFinishNavigation(JNIEnv* env, int tab_id, int page_transition);

  void DidEnterTabSwitcher(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Returns the delegate that routes the events to Java delegates. The
  // returned object is of type:
  // org.chromium.components.visited_url_ranking.url_grouping.DelegateBridge.
  base::android::ScopedJavaLocalRef<jobject> GetJavaDelegateBridge();

 private:
  class SuggestionDelegateBridge;

  // A reference to the Java counterpart of this class. See
  // GroupSuggestionsServiceImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<GroupSuggestionsService> group_suggestions_service_;

  std::unique_ptr<SuggestionDelegateBridge> delegate_bridge_;
};

}  // namespace visited_url_ranking

namespace jni_zero {

// Convert from java UserResponseMetadata.java pointer to native
// UserResponseMetadata object.
template <>
inline visited_url_ranking::GroupSuggestionsDelegate::UserResponseMetadata
FromJniType<
    visited_url_ranking::GroupSuggestionsDelegate::UserResponseMetadata>(
    JNIEnv* env,
    const JavaRef<jobject>& j_metadata) {
  return visited_url_ranking::GroupSuggestionsServiceAndroid::
      ToNativeUserResponse(env, j_metadata);
}

// Convert from native UserResponseMetadata object to a
// UserResponseMetadata.java object pointer.
template <>
inline ScopedJavaLocalRef<jobject>
ToJniType<visited_url_ranking::GroupSuggestionsDelegate::UserResponseMetadata>(
    JNIEnv* env,
    const visited_url_ranking::GroupSuggestionsDelegate::UserResponseMetadata&
        metadata) {
  return visited_url_ranking::GroupSuggestionsServiceAndroid::
      FromNativeUserResponse(env, metadata);
}
}  // namespace jni_zero

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_ANDROID_GROUP_SUGGESTIONS_SERVICE_ANDROID_H_
