// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"

class TabModel;

namespace ui {
class WindowAndroid;
}

namespace autofill {

class AutofillSaveCardDelegateAndroid;
struct AutofillSaveCardUiInfo;

// Bridge class owned by ChromeAutofillClient providing an entry point
// to trigger the save card bottom sheet on Android.
class AutofillSaveCardBottomSheetBridge {
 public:
  // The window and tab model must not be null.
  AutofillSaveCardBottomSheetBridge(ui::WindowAndroid* window_android,
                                    TabModel* tab_model);

  AutofillSaveCardBottomSheetBridge(const AutofillSaveCardBottomSheetBridge&) =
      delete;
  AutofillSaveCardBottomSheetBridge& operator=(
      const AutofillSaveCardBottomSheetBridge&) = delete;

  virtual ~AutofillSaveCardBottomSheetBridge();

  // Requests to show the save card bottom sheet.
  // Overridden in tests.
  virtual void RequestShowContent(
      const AutofillSaveCardUiInfo& ui_info,
      std::unique_ptr<AutofillSaveCardDelegateAndroid> delegate);

  // Hides the save card bottom sheet. The reason for closing it will be set as
  // BottomSheetController.StateChangeReason.INTERACTION_COMPLETE.
  virtual void Hide();

  // -- JNI calls bridged to AutofillSaveCardDelegate --
  // Called when the UI is shown.
  void OnUiShown(JNIEnv* env);
  // Called when the user has accepted the prompt.
  void OnUiAccepted(JNIEnv* env);
  // Called when the user explicitly cancelled the prompt.
  void OnUiCanceled(JNIEnv* env);
  // Called if the user has ignored the prompt.
  void OnUiIgnored(JNIEnv* env);

 protected:
  // Used in tests to inject dependencies.
  explicit AutofillSaveCardBottomSheetBridge(
      base::android::ScopedJavaGlobalRef<jobject>
          java_autofill_save_card_bottom_sheet_bridge);

 private:
  void ResetSaveCardDelegate();

  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_save_card_bottom_sheet_bridge_;
  std::unique_ptr<AutofillSaveCardDelegateAndroid> save_card_delegate_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE_H_
