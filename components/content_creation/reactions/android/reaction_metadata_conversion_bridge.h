// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_METADATA_CONVERSION_BRIDGE_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_METADATA_CONVERSION_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "components/content_creation/reactions/core/reaction_service.h"

using base::android::ScopedJavaLocalRef;

namespace content_creation {

class ReactionMetadata;

class ReactionMetadataConversionBridge {
 public:
  static ScopedJavaLocalRef<jobject> CreateJavaReactionMetadataList(
      JNIEnv* env,
      const std::vector<ReactionMetadata>& metadata);
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_METADATA_CONVERSION_BRIDGE_H_
