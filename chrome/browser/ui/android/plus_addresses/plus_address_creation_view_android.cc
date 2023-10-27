// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"

#include "base/android/jni_string.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/plus_addresses/jni_headers/PlusAddressCreationViewBridge_jni.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {
PlusAddressCreationViewAndroid::PlusAddressCreationViewAndroid(
    base::WeakPtr<PlusAddressCreationController> controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {}

PlusAddressCreationViewAndroid::~PlusAddressCreationViewAndroid() {
  if (java_object_) {
    Java_PlusAddressCreationViewBridge_destroy(
        base::android::AttachCurrentThread(), java_object_);
  }
}

void PlusAddressCreationViewAndroid::Show(
    const std::string& primary_email_address,
    const std::string& plus_address) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_PlusAddressCreationViewBridge_create(
      env, reinterpret_cast<intptr_t>(this),
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject()));

  // TODO(b/303054310): Once project exigencies allow for it, convert all of
  // these back to the android view XML.
  base::android::ScopedJavaLocalRef<jstring> j_title =
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_TITLE));

  base::android::ScopedJavaLocalRef<jstring> j_formatted_description =
      base::android::ConvertUTF8ToJavaString(
          env, l10n_util::GetStringFUTF8(
                   IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_DESCRIPTION_V2,
                   base::UTF8ToUTF16(primary_email_address)));

  base::android::ScopedJavaLocalRef<jstring>
      j_proposed_plus_address_placeholder =
          base::android::ConvertUTF8ToJavaString(env, plus_address);

  base::android::ScopedJavaLocalRef<jstring> j_plus_address_modal_ok =
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_OK_TEXT));

  base::android::ScopedJavaLocalRef<jstring> j_plus_address_modal_cancel =
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT));

  Java_PlusAddressCreationViewBridge_show(
      env, java_object_, j_title, j_formatted_description,
      j_proposed_plus_address_placeholder, j_plus_address_modal_ok,
      j_plus_address_modal_cancel);
}

void PlusAddressCreationViewAndroid::OnConfirmed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnConfirmed();
}

void PlusAddressCreationViewAndroid::OnCanceled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnCanceled();
}

void PlusAddressCreationViewAndroid::PromptDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnDialogDestroyed();
}
}  // namespace plus_addresses
