// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/safe_browsing/suspicious_site_dialog_view_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/safe_browsing/android/suspicious_site_controller_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/safe_browsing/jni_headers/SafeBrowsingSuspiciousSiteDialogBridge_jni.h"

namespace safe_browsing {

SuspiciousSiteDialogViewAndroid::SuspiciousSiteDialogViewAndroid(
    SuspiciousSiteControllerAndroid& controller)
    : controller_(controller) {}

SuspiciousSiteDialogViewAndroid::~SuspiciousSiteDialogViewAndroid() {
  Java_SafeBrowsingSuspiciousSiteDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void SuspiciousSiteDialogViewAndroid::Show(ui::WindowAndroid& window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_SafeBrowsingSuspiciousSiteDialogBridge_create(
      env, window_android.GetJavaObject(), reinterpret_cast<intptr_t>(this)));

  Java_SafeBrowsingSuspiciousSiteDialogBridge_showDialog(
      env, java_object_, controller_->GetTitle(),
      controller_->GetWarningDetailText(), controller_->GetPrimaryButtonText(),
      controller_->GetSecondaryButtonText());
}

void SuspiciousSiteDialogViewAndroid::ContinueAnyway(JNIEnv* env) {
  controller_->OnContinueButtonClicked();
}

void SuspiciousSiteDialogViewAndroid::GoBack(JNIEnv* env) {
  controller_->HandleBackNavigation(
      safe_browsing::SuspiciousSiteWarningUserInteraction::kBackToSafetyButton);
}

void SuspiciousSiteDialogViewAndroid::OnLearnMoreClicked(JNIEnv* env) {
  controller_->OnHelpCenterLinkClicked();
}

void SuspiciousSiteDialogViewAndroid::Close(
    JNIEnv* env,
    ui::ModalDialogWrapper::DismissalCause dismissalCause) {
  controller_->CloseDialog(dismissalCause);
}

static void
JNI_SafeBrowsingSuspiciousSiteDialogBridge_CreateControllerForTesting(  // IN-TEST
    JNIEnv* env,
    content::WebContents* web_contents) {
  safe_browsing::SuspiciousSiteControllerAndroid::CreateForWebContents(
      web_contents);
}

}  // namespace safe_browsing

DEFINE_JNI(SafeBrowsingSuspiciousSiteDialogBridge)
