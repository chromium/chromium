// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/infobar_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "chrome/android/chrome_jni_headers/InfoBar_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

// InfoBarAndroid -------------------------------------------------------------

InfoBarAndroid::InfoBarAndroid(
    std::unique_ptr<infobars::InfoBarDelegate> delegate)
    : infobars::InfoBar(std::move(delegate)) {}

InfoBarAndroid::~InfoBarAndroid() {
  if (!java_info_bar_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InfoBar_onNativeDestroyed(env, java_info_bar_);
  }
}

void InfoBarAndroid::ReassignJavaInfoBar(InfoBarAndroid* replacement) {
  DCHECK(replacement);
  if (!java_info_bar_.is_null()) {
    replacement->SetJavaInfoBar(java_info_bar_);
    java_info_bar_.Reset();
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
    return; // We're closing; don't call anything, it might access the owner.
  delegate()->InfoBarDismissed();
  RemoveSelf();
}

void InfoBarAndroid::CloseJavaInfoBar() {
  if (!java_info_bar_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InfoBar_closeInfoBar(env, java_info_bar_);
    java_info_bar_.Reset(nullptr);
  }
}

int InfoBarAndroid::GetEnumeratedIconId() {
  return ResourceMapper::MapFromChromiumId(delegate()->GetIconId());
}
