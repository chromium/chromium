// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/android/ads_blocked_dialog.h"

#include <stdint.h>

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/subresource_filter/android/subresource_filter_jni_headers/AdsBlockedDialog_jni.h"

AdsBlockedDialogBase::~AdsBlockedDialogBase() = default;

// static
std::unique_ptr<AdsBlockedDialogBase> AdsBlockedDialog::Create(
    content::WebContents* web_contents,
    base::OnceClosure allow_ads_clicked_callback,
    base::OnceClosure learn_more_clicked_callback,
    base::OnceClosure dialog_dismissed_callback) {
  CHECK(web_contents, base::NotFatalUntil::M129);

  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android)
    return nullptr;
  return base::WrapUnique(new AdsBlockedDialog(
      window_android->GetJavaObject(), std::move(allow_ads_clicked_callback),
      std::move(learn_more_clicked_callback),
      std::move(dialog_dismissed_callback)));
}

AdsBlockedDialog::AdsBlockedDialog(
    base::android::ScopedJavaLocalRef<jobject> j_window_android,
    base::OnceClosure allow_ads_clicked_callback,
    base::OnceClosure learn_more_clicked_callback,
    base::OnceClosure dialog_dismissed_callback)
    : allow_ads_clicked_callback_(std::move(allow_ads_clicked_callback)),
      learn_more_clicked_callback_(std::move(learn_more_clicked_callback)),
      dialog_dismissed_callback_(std::move(dialog_dismissed_callback)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ads_blocked_dialog_ = Java_AdsBlockedDialog_create(
      env, reinterpret_cast<intptr_t>(this), j_window_android);
}

AdsBlockedDialog::~AdsBlockedDialog() {
  // When the owning class is destroyed, ensure that any active dialog
  // associated with the class is dismissed.
  if (java_ads_blocked_dialog_)
    Dismiss();
}

void AdsBlockedDialog::Show(bool should_post_dialog) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AdsBlockedDialog_show(env, java_ads_blocked_dialog_, should_post_dialog);
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
  java_ads_blocked_dialog_.Reset();
  std::move(dialog_dismissed_callback_).Run();
}
