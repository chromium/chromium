// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_ANDROID_MESSAGING_CONVERSION_UTILS_H_
#define COMPONENTS_COLLABORATION_INTERNAL_ANDROID_MESSAGING_CONVERSION_UTILS_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/message.h"

namespace collaboration::messaging::android {

// Helper method to convert a PersistentMessage C++ object to a
// PersistentMessage Java object.
base::android::ScopedJavaLocalRef<jobject> PersistentMessageToJava(
    JNIEnv* env,
    const PersistentMessage& message);

// Helper method to convert a PersistentMessage C++ list to a
// List<PersistentMessage> Java object.
base::android::ScopedJavaLocalRef<jobject> PersistentMessagesToJava(
    JNIEnv* env,
    const std::vector<PersistentMessage>& messages);

// Helper method to convert an InstantMessage C++ object to a
// InstantMessage Java object.
base::android::ScopedJavaLocalRef<jobject> InstantMessageToJava(
    JNIEnv* env,
    const InstantMessage& message);

// Helper method to convert a ActivityLogItem C++ list to a
// List<ActivityLogItem> Java object.
base::android::ScopedJavaLocalRef<jobject> ActivityLogItemsToJava(
    JNIEnv* env,
    const std::vector<ActivityLogItem>& activity_log_items);

}  // namespace collaboration::messaging::android

#endif  // COMPONENTS_COLLABORATION_INTERNAL_ANDROID_MESSAGING_CONVERSION_UTILS_H_
