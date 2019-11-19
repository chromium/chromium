// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/interventions/framebust_block_message_delegate_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"

using base::android::ScopedJavaLocalRef;
using base::android::JavaParamRef;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

FramebustBlockMessageDelegateBridge::FramebustBlockMessageDelegateBridge(
    std::unique_ptr<FramebustBlockMessageDelegate> delegate)
    : message_delegate_(std::move(delegate)) {}

FramebustBlockMessageDelegateBridge::~FramebustBlockMessageDelegateBridge() =
    default;

ScopedJavaLocalRef<jstring> FramebustBlockMessageDelegateBridge::GetLongMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF16ToJavaString(env, message_delegate_->GetLongMessage());
}

ScopedJavaLocalRef<jstring>
FramebustBlockMessageDelegateBridge::GetShortMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF16ToJavaString(env, message_delegate_->GetShortMessage());
}

ScopedJavaLocalRef<jstring> FramebustBlockMessageDelegateBridge::GetBlockedUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(env,
                                 message_delegate_->GetBlockedUrl().spec());
}

jint FramebustBlockMessageDelegateBridge::GetEnumeratedIcon(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ResourceMapper::MapFromChromiumId(message_delegate_->GetIconId());
}

void FramebustBlockMessageDelegateBridge::OnLinkTapped(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  message_delegate_->OnLinkClicked();
}
