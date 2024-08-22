// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <stdint.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/omnibox/browser/jni_headers/AutocompleteResult_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaBooleanArray;
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;

namespace {
// Special value passed to VerifyCoherency() suggesting that the action
// requesting verification has no specific index associated with it.
constexpr const int kNoMatchIndex = -1;

// Used for histograms, append only.
enum class MatchVerificationResult {
  VALID_MATCH = 0,
  WRONG_MATCH = 1,
  BAD_RESULT_SIZE = 2,
  OBSOLETE_NATIVE_MATCH_DEAD = 3,
  INVALID_MATCH_POSITION = 4,
  // Keep as the last entry:
  COUNT
};

enum class MatchVerificationPoint {
  INVALID = 0,
  SELECT_MATCH = 1,
  UPDATE_MATCH = 2,
  DELETE_MATCH = 3,
  GROUP_BY_SEARCH_VS_URL_BEFORE = 4,
  GROUP_BY_SEARCH_VS_URL_AFTER = 5,
  ON_TOUCH_MATCH = 6,
  GET_MATCHING_TAB = 7,
};

const char* MatchVerificationPointToString(int verification_point) {
  switch (static_cast<MatchVerificationPoint>(verification_point)) {
    case MatchVerificationPoint::SELECT_MATCH:
      return "Select";
    case MatchVerificationPoint::UPDATE_MATCH:
      return "Update";
    case MatchVerificationPoint::DELETE_MATCH:
      return "Delete";
    case MatchVerificationPoint::GROUP_BY_SEARCH_VS_URL_BEFORE:
      return "Group/Before";
    case MatchVerificationPoint::GROUP_BY_SEARCH_VS_URL_AFTER:
      return "Group/After";
    case MatchVerificationPoint::ON_TOUCH_MATCH:
      return "OnTouch";
    case MatchVerificationPoint::GET_MATCHING_TAB:
      return "GetMatchingTab";
    case MatchVerificationPoint::INVALID:
      return "Invalid";
  }
  NOTREACHED_IN_MIGRATION();
}

bool sInvalidMatchMetricsUploaded = false;

void ReportInvalidMatchData(std::string debug_info, int verification_point) {
  if (sInvalidMatchMetricsUploaded)
    return;

  sInvalidMatchMetricsUploaded = true;

  SCOPED_CRASH_KEY_STRING32("ACMatch", "wrong-match-info", debug_info);
  SCOPED_CRASH_KEY_STRING32("ACMatch", "verification-point",
                            MatchVerificationPointToString(verification_point));
  base::debug::DumpWithoutCrashing();
}
}  // namespace

ScopedJavaLocalRef<jobject> AutocompleteResult::GetOrCreateJavaObject(
    JNIEnv* env) const {
  // Short circuit if we already built the java object.
  if (java_result_)
    return ScopedJavaLocalRef<jobject>(java_result_);

  const size_t groups_count = suggestion_groups_map().size();

  std::vector<int> group_ids(groups_count);
  omnibox::GroupsInfo groups_info;
  std::string serialized_groups_info;

  for (const auto& suggestion_group : suggestion_groups_map()) {
    (*groups_info.mutable_group_configs())[suggestion_group.first] =
        suggestion_group.second;
  }
  if (!groups_info.SerializeToString(&serialized_groups_info)) {
    serialized_groups_info.clear();
  }

  ScopedJavaLocalRef<jintArray> j_group_ids = ToJavaIntArray(env, group_ids);

  java_result_ = Java_AutocompleteResult_fromNative(
      env, reinterpret_cast<intptr_t>(this), BuildJavaMatches(env),
      ToJavaByteArray(env, serialized_groups_info));

  return ScopedJavaLocalRef<jobject>(java_result_);
}

void AutocompleteResult::DestroyJavaObject() const {
  if (!java_result_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutocompleteResult_notifyNativeDestroyed(env, java_result_);
  java_result_.Reset();
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

bool AutocompleteResult::VerifyCoherency(
    JNIEnv* env,
    const JavaParamRef<jlongArray>& j_matches_array,
    jint match_index,
    jint verification_point) {
  DCHECK(j_matches_array);

  std::vector<jlong> j_matches;
  base::android::JavaLongArrayToLongVector(env, j_matches_array, &j_matches);

  if (j_matches.size() != size()) {
    UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                              MatchVerificationResult::BAD_RESULT_SIZE,
                              MatchVerificationResult::COUNT);
    ReportInvalidMatchData(base::NumberToString(j_matches.size()) +
                               "!=" + base::NumberToString(size()),
                           verification_point);
    return false;
  }

  if (match_index != kNoMatchIndex && match_index >= static_cast<int>(size())) {
    UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                              MatchVerificationResult::INVALID_MATCH_POSITION,
                              MatchVerificationResult::COUNT);
    ReportInvalidMatchData(
        base::NumberToString(match_index) + ">=" + base::NumberToString(size()),
        verification_point);
    return false;
  }

  for (auto index = 0u; index < size(); index++) {
    if (reinterpret_cast<intptr_t>(match_at(index)) != j_matches[index]) {
      UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                                MatchVerificationResult::WRONG_MATCH,
                                MatchVerificationResult::COUNT);
      // Note: the NDEBUG is defined for release / debug-disabled builds.
#ifndef NDEBUG
      // Print the list of matches at every position on each side.
      // Used for debugging purposes.
      for (auto i = 0u; i < size(); i++) {
        auto* this_match = match_at(i);
        auto* other_match = reinterpret_cast<AutocompleteMatch*>(j_matches[i]);
        DLOG(WARNING) << "Suggestion at index " << i << ": "
                      << "(Native): " << this_match->fill_into_edit
                      << "(Java): "
                      << (other_match ? other_match->fill_into_edit
                                      : u"<null>");
      }
#endif

      ReportInvalidMatchData(
          base::NumberToString(index) + "/" + base::NumberToString(size()),
          verification_point);
      return false;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                            MatchVerificationResult::VALID_MATCH,
                            MatchVerificationResult::COUNT);
  return true;
}
