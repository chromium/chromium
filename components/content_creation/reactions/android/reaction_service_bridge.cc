// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/reactions/android/reaction_service_bridge.h"

#include "base/android/callback_android.h"
#include "base/bind.h"
#include "components/content_creation/reactions/android/jni_headers/ReactionServiceBridge_jni.h"
#include "components/content_creation/reactions/android/reaction_metadata_conversion_bridge.h"
#include "components/content_creation/reactions/core/reaction_metadata.h"
#include "components/content_creation/reactions/core/reaction_service.h"

namespace content_creation {

namespace {

using ::base::android::AttachCurrentThread;

const char kReactionServiceBridgeKey[] = "reaction_service_bridge";

}  // namespace

// static
ScopedJavaLocalRef<jobject> ReactionServiceBridge::GetBridgeForReactionService(
    ReactionService* reaction_service) {
  DCHECK(reaction_service);
  if (!reaction_service->GetUserData(kReactionServiceBridgeKey)) {
    reaction_service->SetUserData(
        kReactionServiceBridgeKey,
        std::make_unique<ReactionServiceBridge>(reaction_service));
  }

  ReactionServiceBridge* bridge = static_cast<ReactionServiceBridge*>(
      reaction_service->GetUserData(kReactionServiceBridgeKey));
  return ScopedJavaLocalRef<jobject>(bridge->java_obj_);
}

ReactionServiceBridge::ReactionServiceBridge(ReactionService* reaction_service)
    : reaction_service_(reaction_service) {
  DCHECK(reaction_service_);
  JNIEnv* env = AttachCurrentThread();
  java_obj_.Reset(env, Java_ReactionServiceBridge_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

ReactionServiceBridge::~ReactionServiceBridge() {
  JNIEnv* env = AttachCurrentThread();
  Java_ReactionServiceBridge_clearNativePtr(env, java_obj_);
}

void ReactionServiceBridge::GetReactions(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_callback) {
  RunObjectCallbackAndroid(
      j_callback,
      ReactionMetadataConversionBridge::CreateJavaReactionMetadataList(
          env, reaction_service_->GetReactions()));
}

}  // namespace content_creation
