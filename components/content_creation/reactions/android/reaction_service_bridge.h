// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_SERVICE_BRIDGE_H_
#define COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_SERVICE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace content_creation {

class ReactionService;

class ReactionServiceBridge : public base::SupportsUserData::Data {
 public:
  static ScopedJavaLocalRef<jobject> GetBridgeForReactionService(
      ReactionService* reaction_service);

  explicit ReactionServiceBridge(ReactionService* reaction_service);
  ~ReactionServiceBridge() override;

  // Not copyable or movable.
  ReactionServiceBridge(const ReactionServiceBridge&) = delete;
  ReactionServiceBridge& operator=(const ReactionServiceBridge&) = delete;

  void GetReactions(JNIEnv* env,
                    const JavaParamRef<jobject>& jcaller,
                    const JavaParamRef<jobject>& jcallback);

 private:
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<ReactionService> reaction_service_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_REACTIONS_ANDROID_REACTION_SERVICE_BRIDGE_H_
