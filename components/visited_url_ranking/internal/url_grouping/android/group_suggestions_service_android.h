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

  void DidAddTab(JNIEnv* env, int tab_id, int type);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // A reference to the Java counterpart of this class. See
  // GroupSuggestionsServiceImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<GroupSuggestionsService> group_suggestions_service_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_ANDROID_GROUP_SUGGESTIONS_SERVICE_ANDROID_H_
