// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_METADATA_CONVERSION_BRIDGE_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_METADATA_CONVERSION_BRIDGE_H_

#include "base/android/jni_android.h"
#include "components/content_creation/reactions/core/reaction_metadata.h"

using base::android::ScopedJavaLocalRef;

namespace content_creation {

class ReactionMetadataConversionBridge {
 public:
  static ScopedJavaLocalRef<jobject> CreateJavaReactionMetadata(
      JNIEnv* env,
      const ReactionMetadata& metadata);
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_METADATA_CONVERSION_BRIDGE_H_
