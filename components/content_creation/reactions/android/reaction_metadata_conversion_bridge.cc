// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/android/reaction_metadata_conversion_bridge.h"

#include "base/android/jni_string.h"
#include "components/content_creation/reactions/android/jni_headers/ReactionMetadataConversionBridge_jni.h"
#include "components/content_creation/reactions/core/reaction_metadata.h"
#include "components/content_creation/reactions/core/reaction_types.h"

namespace content_creation {

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

ScopedJavaLocalRef<jobject> CreateJavaMetadataAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const ReactionMetadata& reaction_metadata) {
  return Java_ReactionMetadataConversionBridge_createMetadataAndMaybeAddToList(
      env, jlist, static_cast<uint32_t>(reaction_metadata.type()),
      ConvertUTF8ToJavaString(env, reaction_metadata.localized_name()),
      ConvertUTF8ToJavaString(env, reaction_metadata.thumbnail_url()),
      ConvertUTF8ToJavaString(env, reaction_metadata.asset_url()),
      reaction_metadata.frame_count());
}

}  // namespace

// static
ScopedJavaLocalRef<jobject>
ReactionMetadataConversionBridge::CreateJavaReactionMetadataList(
    JNIEnv* env,
    const std::vector<ReactionMetadata>& metadata) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_ReactionMetadataConversionBridge_createReactionList(env);

  for (const auto& reaction_metadata : metadata) {
    CreateJavaMetadataAndMaybeAddToList(env, jlist, reaction_metadata);
  }

  return jlist;
}

}  // namespace content_creation
