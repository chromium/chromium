// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/android/window_android.h"

namespace autofill {

// Bridge class owned by ChromeAutofillClient providing an entry point
// to trigger the save card bottom sheet on Android.
class AutofillSaveCardBottomSheetBridge {
 public:
  explicit AutofillSaveCardBottomSheetBridge(content::WebContents* contents);

  AutofillSaveCardBottomSheetBridge(const AutofillSaveCardBottomSheetBridge&) =
      delete;
  AutofillSaveCardBottomSheetBridge& operator=(
      const AutofillSaveCardBottomSheetBridge&) = delete;

  virtual ~AutofillSaveCardBottomSheetBridge();

  // Requests to show the save card bottom sheet.
  // Returns true if the bottom sheet was shown.
  // Overridden in tests.
  virtual bool RequestShowContent();

 protected:
  // Used in tests to inject dependencies.
  explicit AutofillSaveCardBottomSheetBridge(
      base::android::ScopedJavaGlobalRef<jobject>
          java_autofill_save_card_bottom_sheet_bridge);

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_autofill_save_card_bottom_sheet_bridge_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE_H_
