// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/omnibox/browser/jni_headers/AutocompleteMatch_jni.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/query_tiles/android/tile_conversion_bridge.h"
#include "url/android/gurl_android.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;

// static
jclass AutocompleteMatch::GetClazz(JNIEnv* env) {
  return org_chromium_components_omnibox_AutocompleteMatch_clazz(env);
}

ScopedJavaLocalRef<jobject> AutocompleteMatch::GetOrCreateJavaObject(
    JNIEnv* env) const {
  // Short circuit if we already built the match.
  if (java_match_)
    return ScopedJavaLocalRef<jobject>(java_match_);

  std::vector<int> contents_class_offsets;
  std::vector<int> contents_class_styles;
  for (auto contents_class : contents_class) {
    contents_class_offsets.push_back(contents_class.offset);
    contents_class_styles.push_back(contents_class.style);
  }

  std::vector<int> description_class_offsets;
  std::vector<int> description_class_styles;
  for (auto description_class : description_class) {
    description_class_offsets.push_back(description_class.offset);
    description_class_styles.push_back(description_class.style);
  }

  base::android::ScopedJavaLocalRef<jobject> janswer;
  if (answer)
    janswer = answer->CreateJavaObject();
  ScopedJavaLocalRef<jstring> j_image_dominant_color;
  ScopedJavaLocalRef<jstring> j_post_content_type;
  ScopedJavaLocalRef<jbyteArray> j_post_content;
  std::string clipboard_image_data;

  if (!image_dominant_color.empty()) {
    j_image_dominant_color = ConvertUTF8ToJavaString(env, image_dominant_color);
  }

  if (post_content && !post_content->first.empty() &&
      !post_content->second.empty()) {
    j_post_content_type = ConvertUTF8ToJavaString(env, post_content->first);
    j_post_content = ToJavaByteArray(env, post_content->second);
  }

  if (search_terms_args.get()) {
    clipboard_image_data = search_terms_args->image_thumbnail_content;
  }

  ScopedJavaLocalRef<jobject> j_query_tiles =
      query_tiles::TileConversionBridge::CreateJavaTiles(env, query_tiles);

  std::vector<base::string16> navsuggest_titles;
  navsuggest_titles.reserve(navsuggest_tiles.size());
  std::vector<base::android::ScopedJavaLocalRef<jobject>> navsuggest_urls;
  navsuggest_urls.reserve(navsuggest_tiles.size());
  for (const auto& tile : navsuggest_tiles) {
    navsuggest_titles.push_back(tile.title);
    navsuggest_urls.push_back(url::GURLAndroid::FromNativeGURL(env, tile.url));
  }

  std::vector<int> temp_subtypes(subtypes.begin(), subtypes.end());

  java_match_ = Java_AutocompleteMatch_build(
      env, type, ToJavaIntArray(env, temp_subtypes),
      AutocompleteMatch::IsSearchType(type), relevance, transition,
      ConvertUTF16ToJavaString(env, contents),
      ToJavaIntArray(env, contents_class_offsets),
      ToJavaIntArray(env, contents_class_styles),
      ConvertUTF16ToJavaString(env, description),
      ToJavaIntArray(env, description_class_offsets),
      ToJavaIntArray(env, description_class_styles), janswer,
      ConvertUTF16ToJavaString(env, fill_into_edit),
      url::GURLAndroid::FromNativeGURL(env, destination_url),
      url::GURLAndroid::FromNativeGURL(env, image_url), j_image_dominant_color,
      SupportsDeletion(), j_post_content_type, j_post_content,
      suggestion_group_id.value_or(
          SearchSuggestionParser::kNoSuggestionGroupId),
      j_query_tiles, ToJavaByteArray(env, clipboard_image_data), has_tab_match,
      ToJavaArrayOfStrings(env, navsuggest_titles),
      url::GURLAndroid::ToJavaArrayOfGURLs(env, navsuggest_urls));

  return ScopedJavaLocalRef<jobject>(java_match_);
}
