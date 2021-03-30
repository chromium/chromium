// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <stdint.h>

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
  std::vector<std::u16string> group_names(groups_count);
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

  java_result_ = Java_AutocompleteResult_build(
      env, reinterpret_cast<intptr_t>(this), BuildJavaMatches(env), j_group_ids,
      j_group_names, j_group_collapsed_states);

  return ScopedJavaLocalRef<jobject>(java_result_);
}

ScopedJavaLocalRef<jobjectArray> AutocompleteResult::BuildJavaMatches(
    JNIEnv* env) const {
  jclass clazz = AutocompleteMatch::GetClazz(env);
  ScopedJavaLocalRef<jobjectArray> j_matches(
      env, env->NewObjectArray(matches_.size(), clazz, nullptr));
  base::android::CheckException(env);

  for (size_t index = 0; index < matches_.size(); ++index) {
    env->SetObjectArrayElement(
        j_matches.obj(), index,
        matches_[index].GetOrCreateJavaObject(env).obj());
  }

  return j_matches;
}

void AutocompleteResult::GroupSuggestionsBySearchVsURL(JNIEnv* env,
                                                       int first_index,
                                                       int last_index) {
  const int num_elements = matches_.size();
  DCHECK_GE(first_index, 0);
  DCHECK_LT(first_index, num_elements);
  DCHECK_GT(last_index, 0);
  DCHECK_LE(last_index, num_elements);
  DCHECK_LT(first_index, last_index);
  auto range_start = const_cast<ACMatches&>(matches_).begin();
  GroupSuggestionsBySearchVsURL(range_start + first_index,
                                range_start + last_index);
  Java_AutocompleteResult_updateMatches(env, java_result_,
                                        BuildJavaMatches(env));
}
