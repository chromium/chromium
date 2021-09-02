// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/android/ads_blocked_dialog.h"

#include <stdint.h>

#include <utility>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/subresource_filter/android/subresource_filter_jni_headers/AdsBlockedDialog_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

AdsBlockedDialogBase::~AdsBlockedDialogBase() = default;

// static
std::unique_ptr<AdsBlockedDialogBase> AdsBlockedDialog::Create(
    content::WebContents* web_contents,
    AllowAdsClickedCallback allow_ads_clicked_callback,
    LearnMoreClickedCallback learn_more_clicked_callback) {
  DCHECK(web_contents);

  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android)
    return nullptr;
  return base::WrapUnique(new AdsBlockedDialog(
      window_android->GetJavaObject(), std::move(allow_ads_clicked_callback),
      std::move(learn_more_clicked_callback)));
}

AdsBlockedDialog::AdsBlockedDialog(
    base::android::ScopedJavaLocalRef<jobject> j_window_android,
    AllowAdsClickedCallback allow_ads_clicked_callback,
    LearnMoreClickedCallback learn_more_clicked_callback)
    : allow_ads_clicked_callback_(std::move(allow_ads_clicked_callback)),
      learn_more_clicked_callback_(std::move(learn_more_clicked_callback)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ads_blocked_dialog_ = Java_AdsBlockedDialog_create(
      env, reinterpret_cast<intptr_t>(this), j_window_android);
}

AdsBlockedDialog::~AdsBlockedDialog() {
  DCHECK(java_ads_blocked_dialog_.is_null());
}

void AdsBlockedDialog::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AdsBlockedDialog_show(env, java_ads_blocked_dialog_);
}

void AdsBlockedDialog::Dismiss() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AdsBlockedDialog_dismiss(env, java_ads_blocked_dialog_);
}

void AdsBlockedDialog::OnAllowAdsClicked(JNIEnv* env) {
  std::move(allow_ads_clicked_callback_).Run();
}

void AdsBlockedDialog::OnLearnMoreClicked(JNIEnv* env) {
  std::move(learn_more_clicked_callback_).Run();
}

void AdsBlockedDialog::OnDismissed(JNIEnv* env) {
  // TODO(aishwaryarj): destroy the dialog instance on dismissal.
}
