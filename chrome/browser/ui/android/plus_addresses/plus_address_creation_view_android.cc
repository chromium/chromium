// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/plus_addresses/jni_headers/PlusAddressCreationViewBridge_jni.h"

namespace plus_addresses {

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

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

void PlusAddressCreationViewAndroid::ShowInit(
    const std::string& primary_email_address,
    bool refresh_supported,
    bool has_accepted_notice) {
  JNIEnv* env = base::android::AttachCurrentThread();
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents_);
  if (!tab_model) {
    // TODO(crbug.com/40276862): Verify expected behavior in this case.
    return;
  }

  java_object_.Reset(Java_PlusAddressCreationViewBridge_create(
      env, reinterpret_cast<intptr_t>(this),
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
      tab_model->GetJavaObject()));

  // TODO(b/303054310): Once project exigencies allow for it, convert all of
  // these back to the android view XML.
  ScopedJavaLocalRef<jstring> j_title;
  ScopedJavaLocalRef<jstring> j_formatted_description;
  ScopedJavaLocalRef<jstring> j_formatted_notice;
  ScopedJavaLocalRef<jstring> j_plus_address_modal_cancel;

  if (!has_accepted_notice) {
    j_title = ConvertUTF16ToJavaString(
        env, l10n_util::GetStringUTF16(
                 IDS_PLUS_ADDRESS_BOTTOMSHEET_TITLE_NOTICE_ANDROID));
    j_formatted_description = ConvertUTF8ToJavaString(
        env, l10n_util::GetStringUTF8(
                 IDS_PLUS_ADDRESS_BOTTOMSHEET_DESCRIPTION_NOTICE_ANDROID));
    j_formatted_notice = ConvertUTF8ToJavaString(
        env,
        l10n_util::GetStringFUTF8(IDS_PLUS_ADDRESS_BOTTOMSHEET_NOTICE_ANDROID,
                                  base::UTF8ToUTF16(primary_email_address)));
    j_plus_address_modal_cancel = ConvertUTF16ToJavaString(
        env, l10n_util::GetStringUTF16(
                 IDS_PLUS_ADDRESS_BOTTOMSHEET_CANCEL_TEXT_ANDROID));
  } else {
    j_title = ConvertUTF16ToJavaString(
        env,
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_BOTTOMSHEET_TITLE_ANDROID));
    j_formatted_description = ConvertUTF8ToJavaString(
        env, l10n_util::GetStringFUTF8(
                 IDS_PLUS_ADDRESS_BOTTOMSHEET_DESCRIPTION_ANDROID,
                 base::UTF8ToUTF16(primary_email_address)));
  }

  ScopedJavaLocalRef<jstring> j_proposed_plus_address_placeholder =
      ConvertUTF16ToJavaString(
          env,
          l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_BOTTOMSHEET_PROPOSED_PLUS_ADDRESS_PLACEHOLDER_ANDROID));
  ScopedJavaLocalRef<jstring> j_plus_address_modal_ok =
      ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_PLUS_ADDRESS_BOTTOMSHEET_OK_TEXT_ANDROID));
  ScopedJavaLocalRef<jstring> j_error_report_instruction =
      ConvertUTF16ToJavaString(
          env,
          l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_BOTTOMSHEET_REPORT_ERROR_INSTRUCTION_ANDROID));

  ScopedJavaLocalRef<jstring> j_learn_more_url =
      base::android::ConvertUTF8ToJavaString(
          env, features::kPlusAddressLearnMoreUrl.Get());
  ScopedJavaLocalRef<jstring> j_error_report_url =
      base::android::ConvertUTF8ToJavaString(
          env, features::kPlusAddressErrorReportUrl.Get());
  Java_PlusAddressCreationViewBridge_show(
      env, java_object_, j_title, j_formatted_description, j_formatted_notice,
      j_proposed_plus_address_placeholder, j_plus_address_modal_ok,
      j_plus_address_modal_cancel, j_error_report_instruction, j_learn_more_url,
      j_error_report_url, refresh_supported);
}

void PlusAddressCreationViewAndroid::OnRefreshClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnRefreshClicked();
}

void PlusAddressCreationViewAndroid::OnConfirmRequested(
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

void PlusAddressCreationViewAndroid::ShowReserveResult(
    const PlusProfileOrError& maybe_plus_profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (maybe_plus_profile.has_value()) {
    ScopedJavaLocalRef<jstring> j_proposed_plus_address =
        base::android::ConvertUTF8ToJavaString(
            env, maybe_plus_profile->plus_address);
    Java_PlusAddressCreationViewBridge_updateProposedPlusAddress(
        env, java_object_, j_proposed_plus_address);
  } else {
    Java_PlusAddressCreationViewBridge_showError(env, java_object_);
  }
}

void PlusAddressCreationViewAndroid::ShowConfirmResult(
    const PlusProfileOrError& maybe_plus_profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (maybe_plus_profile.has_value()) {
    Java_PlusAddressCreationViewBridge_finishConfirm(env, java_object_);
  } else {
    Java_PlusAddressCreationViewBridge_showError(env, java_object_);
  }
}

void PlusAddressCreationViewAndroid::HideRefreshButton() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PlusAddressCreationViewBridge_hideRefreshButton(env, java_object_);
}
}  // namespace plus_addresses
