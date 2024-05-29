// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/android/infobar_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/infobars/android/jni_headers/InfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace infobars {

// InfoBarAndroid -------------------------------------------------------------

InfoBarAndroid::InfoBarAndroid(std::unique_ptr<InfoBarDelegate> delegate)
    : InfoBar(std::move(delegate)) {}

InfoBarAndroid::~InfoBarAndroid() {
  if (!java_info_bar_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InfoBar_resetNativeInfoBar(env, java_info_bar_);
  }
}

void InfoBarAndroid::SetJavaInfoBar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  DCHECK(java_info_bar_.is_null());
  java_info_bar_.Reset(java_info_bar);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InfoBar_setNativeInfoBar(env, java_info_bar,
                                reinterpret_cast<intptr_t>(this));
}

const JavaRef<jobject>& InfoBarAndroid::GetJavaInfoBar() {
  return java_info_bar_;
}

bool InfoBarAndroid::HasSetJavaInfoBar() const {
  return !java_info_bar_.is_null();
}

int InfoBarAndroid::GetInfoBarIdentifier(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return delegate()->GetIdentifier();
}

void InfoBarAndroid::OnButtonClicked(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jint action) {
  ProcessButton(action);
}

void InfoBarAndroid::OnCloseButtonClicked(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  delegate()->InfoBarDismissed();
  RemoveSelf();
}

void InfoBarAndroid::CloseJavaInfoBar() {
  if (!java_info_bar_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InfoBar_closeInfoBar(env, java_info_bar_);
    Java_InfoBar_resetNativeInfoBar(env, java_info_bar_);
    java_info_bar_.Reset(nullptr);
  }
}

}  // namespace infobars
