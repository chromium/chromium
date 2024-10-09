// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_view.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_controller.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/plus_address_ui_utils.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/plus_addresses/jni_headers/AllPlusAddressesBottomSheetBridge_jni.h"
#include "chrome/browser/ui/android/plus_addresses/jni_headers/AllPlusAddressesBottomSheetUIInfo_jni.h"
#include "chrome/browser/ui/android/plus_addresses/jni_headers/PlusProfile_jni.h"

namespace plus_addresses {

AllPlusAddressesBottomSheetView::AllPlusAddressesBottomSheetView(
    AllPlusAddressesBottomSheetController* controller)
    : controller_(CHECK_DEREF(controller)) {}

AllPlusAddressesBottomSheetView::~AllPlusAddressesBottomSheetView() {
  if (java_object_internal_) {
    Java_AllPlusAddressesBottomSheetBridge_destroy(
        jni_zero::AttachCurrentThread(), java_object_internal_);
  }
}

void AllPlusAddressesBottomSheetView::Show(
    base::span<const PlusProfile> profiles) {
  base::android::ScopedJavaGlobalRef<jobject> java_object =
      GetOrCreateJavaObject();
  if (!java_object) {
    return;
  }

  JNIEnv* env = jni_zero::AttachCurrentThread();
  std::vector<base::android::ScopedJavaLocalRef<jobject>> java_profiles;
  java_profiles.reserve(profiles.size());

  for (const PlusProfile& profile : profiles) {
    java_profiles.emplace_back(Java_PlusProfile_Constructor(
        env, *profile.plus_address, GetOriginForDisplay(profile),
        profile.facet.canonical_spec()));
  }

  base::android::ScopedJavaLocalRef<jobject> ui_info =
      Java_AllPlusAddressesBottomSheetUIInfo_Constructor(env);
  Java_AllPlusAddressesBottomSheetUIInfo_setTitle(
      env, ui_info,
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_ALL_PLUS_ADDRESSES_BOTTOMSHEET_TITLE_ANDROID));
  Java_AllPlusAddressesBottomSheetUIInfo_setWarning(
      env, ui_info,
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_ALL_PLUS_ADDRESSES_BOTTOMSHEET_WARNING_ANDROID));
  Java_AllPlusAddressesBottomSheetUIInfo_setQueryHint(
      env, ui_info,
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_ALL_PLUS_ADDRESSES_BOTTOMSHEET_QUERY_HINT_ANDROID));
  Java_AllPlusAddressesBottomSheetUIInfo_setPlusProfiles(env, ui_info,
                                                         java_profiles);

  Java_AllPlusAddressesBottomSheetBridge_showPlusAddresses(env, java_object,
                                                           ui_info);
}

void AllPlusAddressesBottomSheetView::OnPlusAddressSelected(
    JNIEnv* env,
    const std::string& plus_address) {
  controller_->OnPlusAddressSelected(plus_address);
}

void AllPlusAddressesBottomSheetView::OnDismissed(JNIEnv* env) {
  controller_->OnBottomSheetDismissed();
}

base::android::ScopedJavaGlobalRef<jobject>
AllPlusAddressesBottomSheetView::GetOrCreateJavaObject() {
  if (java_object_internal_) {
    return java_object_internal_;
  }
  if (!controller_->GetNativeView() ||
      !controller_->GetNativeView()->GetWindowAndroid() ||
      !controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject()) {
    return nullptr;  // No window attached (yet or anymore).
  }
  return java_object_internal_ = Java_AllPlusAddressesBottomSheetBridge_create(
             jni_zero::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
             controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
             controller_->GetProfile()->GetJavaObject());
}

}  // namespace plus_addresses
