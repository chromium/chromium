// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INTERVENTIONS_FRAMEBUST_BLOCK_MESSAGE_DELEGATE_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_INTERVENTIONS_FRAMEBUST_BLOCK_MESSAGE_DELEGATE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"

class FramebustBlockMessageDelegate;

// Wrapper class that exposes data from a native FramebustBlockMessageDelegate
// through JNI.
class FramebustBlockMessageDelegateBridge {
 public:
  explicit FramebustBlockMessageDelegateBridge(
      std::unique_ptr<FramebustBlockMessageDelegate> delegate);

  FramebustBlockMessageDelegateBridge(
      const FramebustBlockMessageDelegateBridge&) = delete;
  FramebustBlockMessageDelegateBridge& operator=(
      const FramebustBlockMessageDelegateBridge&) = delete;

  virtual ~FramebustBlockMessageDelegateBridge();

  // JNI accessors.
  base::android::ScopedJavaLocalRef<jstring> GetLongMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetShortMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetBlockedUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint GetEnumeratedIcon(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void OnLinkTapped(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);

 private:
  std::unique_ptr<FramebustBlockMessageDelegate> message_delegate_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INTERVENTIONS_FRAMEBUST_BLOCK_MESSAGE_DELEGATE_BRIDGE_H_
