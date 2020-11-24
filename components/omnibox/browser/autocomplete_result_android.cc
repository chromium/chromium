// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/omnibox/browser/jni_headers/AutocompleteResult_jni.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/query_tiles/android/tile_conversion_bridge.h"
#include "url/android/gurl_android.h"

using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaBooleanArray;
using base::android::ToJavaIntArray;

ScopedJavaLocalRef<jobject> AutocompleteResult::GetOrCreateJavaObject(
    JNIEnv* env) const {
  // Short circuit if we already built the java object.
  if (java_result_)
    return ScopedJavaLocalRef<jobject>(java_result_);

  const size_t groups_count = headers_map_.size();

  std::vector<int> group_ids(groups_count);
  std::vector<base::string16> group_names(groups_count);
  bool group_collapsed_states[groups_count];

  size_t index = 0;
  for (const auto& group_header : headers_map_) {
    group_ids[index] = group_header.first;
    group_names[index] = group_header.second;
    group_collapsed_states[index] =
        base::Contains(hidden_group_ids_, group_header.first);
    ++index;
  }

  ScopedJavaLocalRef<jintArray> j_group_ids = ToJavaIntArray(env, group_ids);
  ScopedJavaLocalRef<jbooleanArray> j_group_collapsed_states =
      ToJavaBooleanArray(env, group_collapsed_states, groups_count);
  ScopedJavaLocalRef<jobjectArray> j_group_names =
      ToJavaArrayOfStrings(env, group_names);

  jclass clazz = AutocompleteMatch::GetClazz(env);
  ScopedJavaLocalRef<jobjectArray> j_matches(
      env, env->NewObjectArray(matches_.size(), clazz, nullptr));
  base::android::CheckException(env);

  for (index = 0; index < matches_.size(); ++index) {
    env->SetObjectArrayElement(
        j_matches.obj(), index,
        matches_[index].GetOrCreateJavaObject(env).obj());
  }

  java_result_ = Java_AutocompleteResult_build(
      env, j_matches, j_group_ids, j_group_names, j_group_collapsed_states);

  return ScopedJavaLocalRef<jobject>(java_result_);
}
