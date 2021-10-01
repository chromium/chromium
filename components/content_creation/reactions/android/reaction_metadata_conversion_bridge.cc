// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/android/reaction_metadata_conversion_bridge.h"

#include "base/android/jni_string.h"
#include "components/content_creation/reactions/android/jni_headers/ReactionMetadataConversionBridge_jni.h"
#include "components/content_creation/reactions/core/reaction_types.h"

namespace content_creation {

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

// static
ScopedJavaLocalRef<jobject>
ReactionMetadataConversionBridge::CreateJavaReactionMetadata(
    JNIEnv* env,
    const ReactionMetadata& metadata) {
  return Java_ReactionMetadataConversionBridge_createReactionMetadata(
      env, static_cast<uint32_t>(metadata.type()),
      ConvertUTF8ToJavaString(env, metadata.localized_name()),
      ConvertUTF8ToJavaString(env, metadata.thumbnail_url()),
      ConvertUTF8ToJavaString(env, metadata.asset_url()));
}

}  // namespace content_creation
