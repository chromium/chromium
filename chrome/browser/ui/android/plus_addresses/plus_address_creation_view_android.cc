// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/plus_addresses/jni_headers/PlusAddressCreationErrorStateInfo_jni.h"
#include "chrome/browser/ui/android/plus_addresses/jni_headers/PlusAddressCreationNormalStateInfo_jni.h"
#include "chrome/browser/ui/android/plus_addresses/jni_headers/PlusAddressCreationViewBridge_jni.h"

namespace plus_addresses {

namespace {

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

ScopedJavaLocalRef<jobject> GetNormatStateUiInfo(
    const std::string& primary_email_address,
    bool has_accepted_notice) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // TODO(b/303054310): Once project exigencies allow for it, convert all of
  // these back to the android view XML.
  std::u16string title;
  std::u16string formatted_description;
  std::u16string formatted_notice;
  std::u16string plus_address_modal_cancel;

  if (!has_accepted_notice) {
    title = l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_BOTTOMSHEET_TITLE_NOTICE_ANDROID);

    formatted_description = l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_BOTTOMSHEET_DESCRIPTION_NOTICE_ANDROID);

    formatted_notice =
        l10n_util::GetStringFUTF16(IDS_PLUS_ADDRESS_BOTTOMSHEET_NOTICE_ANDROID,
                                   base::UTF8ToUTF16(primary_email_address));

    plus_address_modal_cancel = l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_BOTTOMSHEET_CANCEL_TEXT_ANDROID);
  } else {
    title =
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_BOTTOMSHEET_TITLE_ANDROID);

    formatted_description = l10n_util::GetStringFUTF16(
        IDS_PLUS_ADDRESS_BOTTOMSHEET_DESCRIPTION_ANDROID,
        base::UTF8ToUTF16(primary_email_address));
  }

  std::u16string proposed_plus_address_placeholder = l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_BOTTOMSHEET_PROPOSED_PLUS_ADDRESS_PLACEHOLDER_ANDROID);
  std::u16string plus_address_modal_ok =
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_BOTTOMSHEET_OK_TEXT_ANDROID);
  std::u16string error_report_instruction = l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_BOTTOMSHEET_REPORT_ERROR_INSTRUCTION_ANDROID);

  GURL learn_more_url = GURL(features::kPlusAddressLearnMoreUrl.Get());

  GURL error_report_url = GURL(features::kPlusAddressErrorReportUrl.Get());

  return Java_PlusAddressCreationNormalStateInfo_Constructor(
      env, title, formatted_description, formatted_notice,
      proposed_plus_address_placeholder, plus_address_modal_ok,
      plus_address_modal_cancel, error_report_instruction, learn_more_url,
      error_report_url);
}

}  // namespace

PlusAddressCreationErrorStateInfo::PlusAddressCreationErrorStateInfo(
    PlusAddressCreationBottomSheetErrorType error_type,
    std::u16string title,
    std::u16string description,
    std::u16string ok_text,
    std::u16string cancel_text)
    : error_type(error_type),
      title(title),
      description(description),
      ok_text(ok_text),
      cancel_text(cancel_text) {}

PlusAddressCreationErrorStateInfo::~PlusAddressCreationErrorStateInfo() =
    default;

PlusAddressCreationErrorStateInfo::PlusAddressCreationErrorStateInfo(
    const PlusAddressCreationErrorStateInfo&) = default;
PlusAddressCreationErrorStateInfo::PlusAddressCreationErrorStateInfo(
    PlusAddressCreationErrorStateInfo&&) = default;
PlusAddressCreationErrorStateInfo& PlusAddressCreationErrorStateInfo::operator=(
    const PlusAddressCreationErrorStateInfo&) = default;
PlusAddressCreationErrorStateInfo& PlusAddressCreationErrorStateInfo::operator=(
    PlusAddressCreationErrorStateInfo&&) = default;

PlusAddressCreationViewAndroid::PlusAddressCreationViewAndroid(
    base::WeakPtr<PlusAddressCreationController> controller)
    : controller_(controller) {}

PlusAddressCreationViewAndroid::~PlusAddressCreationViewAndroid() {
  if (java_object_) {
    Java_PlusAddressCreationViewBridge_destroy(
        base::android::AttachCurrentThread(), java_object_);
  }
}

void PlusAddressCreationViewAndroid::ShowInit(
    gfx::NativeView native_view,
    TabModel* tab_model,
    const std::string& primary_email_address,
    bool refresh_supported,
    bool has_accepted_notice) {
  base::android::ScopedJavaGlobalRef<jobject> java_object =
      GetOrCreateJavaObject(native_view, tab_model);
  if (!java_object) {
    return;
  }

  Java_PlusAddressCreationViewBridge_show(
      base::android::AttachCurrentThread(), java_object_,
      GetNormatStateUiInfo(primary_email_address, has_accepted_notice),
      refresh_supported);
}

void PlusAddressCreationViewAndroid::TryAgainToReservePlusAddress(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->TryAgainToReservePlusAddress();
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

void PlusAddressCreationViewAndroid::ShowReservedProfile(
    const PlusProfile& reserved_profile) {
  if (!java_object_) {
    return;
  }
  Java_PlusAddressCreationViewBridge_updateProposedPlusAddress(
      base::android::AttachCurrentThread(), java_object_,
      *reserved_profile.plus_address);
}

void PlusAddressCreationViewAndroid::FinishConfirm() {
  if (!java_object_) {
    return;
  }
  Java_PlusAddressCreationViewBridge_finishConfirm(
      base::android::AttachCurrentThread(), java_object_);
}

void PlusAddressCreationViewAndroid::ShowError(
    PlusAddressCreationErrorStateInfo error_info) {
  if (!java_object_) {
    return;
  }
  ScopedJavaLocalRef<jobject> error_info_java_object;
  if (base::FeatureList::IsEnabled(
          features::kPlusAddressAndroidErrorStatesEnabled)) {
    error_info_java_object = Java_PlusAddressCreationErrorStateInfo_Constructor(
        base::android::AttachCurrentThread(),
        base::to_underlying(error_info.error_type), error_info.title,
        error_info.description, error_info.ok_text, error_info.cancel_text);
  }
  Java_PlusAddressCreationViewBridge_showError(
      base::android::AttachCurrentThread(), java_object_,
      error_info_java_object);
}

void PlusAddressCreationViewAndroid::HideRefreshButton() {
  if (!java_object_) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PlusAddressCreationViewBridge_hideRefreshButton(env, java_object_);
}

base::android::ScopedJavaGlobalRef<jobject>
PlusAddressCreationViewAndroid::GetOrCreateJavaObject(
    gfx::NativeView native_view,
    TabModel* tab_model) {
  if (java_object_) {
    return java_object_;
  }
  if (!tab_model || !native_view || !native_view->GetWindowAndroid()) {
    return nullptr;  // No window attached (yet or anymore).
  }
  return java_object_ = Java_PlusAddressCreationViewBridge_create(
             base::android::AttachCurrentThread(),
             reinterpret_cast<intptr_t>(this),
             native_view->GetWindowAndroid()->GetJavaObject(),
             tab_model->GetJavaObject());
}
}  // namespace plus_addresses
