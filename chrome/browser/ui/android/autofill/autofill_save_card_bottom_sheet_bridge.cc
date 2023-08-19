// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/AutofillSaveCardBottomSheetBridge_jni.h"

namespace autofill {

AutofillSaveCardBottomSheetBridge::AutofillSaveCardBottomSheetBridge(
    content::WebContents* web_contents) {
  auto* window = web_contents->GetNativeView()->GetWindowAndroid();
  java_autofill_save_card_bottom_sheet_bridge_ =
      Java_AutofillSaveCardBottomSheetBridge_Constructor(
          base::android::AttachCurrentThread(), window->GetJavaObject());
}

AutofillSaveCardBottomSheetBridge::~AutofillSaveCardBottomSheetBridge() =
    default;

bool AutofillSaveCardBottomSheetBridge::RequestShowContent() {
  return Java_AutofillSaveCardBottomSheetBridge_requestShowContent(
      base::android::AttachCurrentThread(),
      java_autofill_save_card_bottom_sheet_bridge_);
}

AutofillSaveCardBottomSheetBridge::AutofillSaveCardBottomSheetBridge(
    base::android::ScopedJavaGlobalRef<jobject>
        java_autofill_save_card_bottom_sheet_bridge)
    : java_autofill_save_card_bottom_sheet_bridge_(
          java_autofill_save_card_bottom_sheet_bridge) {}

}  // namespace autofill
