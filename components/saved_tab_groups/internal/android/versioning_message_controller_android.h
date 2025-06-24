// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_ANDROID_VERSIONING_MESSAGE_CONTROLLER_ANDROID_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_ANDROID_VERSIONING_MESSAGE_CONTROLLER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"

using base::android::JavaParamRef;

namespace tab_groups {

// Helper class responsible for bridging the VersioningMessageController between
// C++ and Java.
class VersioningMessageControllerAndroid {
 public:
  explicit VersioningMessageControllerAndroid(
      VersioningMessageController* versioning_message_controller);
  ~VersioningMessageControllerAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

  bool IsInitialized(JNIEnv* env, const JavaParamRef<jobject>& j_caller);
  bool ShouldShowMessageUi(JNIEnv* env,
                           const JavaParamRef<jobject>& j_caller,
                           jint j_message_type);
  void ShouldShowMessageUiAsync(JNIEnv* env,
                                const JavaParamRef<jobject>& j_caller,
                                jint j_message_type,
                                const JavaParamRef<jobject>& j_callback);
  void OnMessageUiShown(JNIEnv* env,
                        const JavaParamRef<jobject>& j_caller,
                        jint j_message_type);
  void OnMessageUiDismissed(JNIEnv* env,
                            const JavaParamRef<jobject>& j_caller,
                            jint j_message_type);

 private:
  // A reference to the Java counterpart of this class.  See
  // VersioningMessageControllerImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned. This is safe because the JNI bridge is destroyed and the
  // native pointer in Java is cleared whenever native is destroyed. Hence there
  // will be no subsequent unsafe calls to native.
  raw_ptr<VersioningMessageController> versioning_message_controller_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_ANDROID_VERSIONING_MESSAGE_CONTROLLER_ANDROID_H_
