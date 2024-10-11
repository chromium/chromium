// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_IBAN_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_IBAN_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include <memory>
#include <string_view>

#include "base/android/scoped_java_ref.h"

class TabModel;

namespace ui {
class WindowAndroid;
}

namespace autofill {

class AutofillSaveIbanDelegate;
struct AutofillSaveIbanUiInfo;

// Bridge class owned by ChromeAutofillClient providing an entry point
// to trigger the save IBAN bottom sheet on Android.
class AutofillSaveIbanBottomSheetBridge {
 public:
  // The window and tab model must not be null.
  AutofillSaveIbanBottomSheetBridge(ui::WindowAndroid* window_android,
                                    TabModel* tab_model);

  AutofillSaveIbanBottomSheetBridge(const AutofillSaveIbanBottomSheetBridge&) =
      delete;
  AutofillSaveIbanBottomSheetBridge& operator=(
      const AutofillSaveIbanBottomSheetBridge&) = delete;

  ~AutofillSaveIbanBottomSheetBridge();

  // Requests to show the save IBAN bottom sheet.
  void RequestShowContent(const AutofillSaveIbanUiInfo& ui_info,
                          std::unique_ptr<AutofillSaveIbanDelegate> delegate);

  void OnUiAccepted(JNIEnv* env, const std::u16string& user_provided_nickname);
  void OnUiCanceled(JNIEnv* env);
  void OnUiIgnored(JNIEnv* env);

 private:
  void ResetSaveIbanDelegate();

  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_save_iban_bottom_sheet_bridge_;
  std::unique_ptr<AutofillSaveIbanDelegate> save_iban_delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_IBAN_BOTTOM_SHEET_BRIDGE_H_
