// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONFIRM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONFIRM_CONTROLLER_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/android/autofill/save_card_message_confirm_delegate.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace content {
class WebContents;
}

namespace autofill {

// Enables the user to fill in or confirm cardholder name or expiration date.
// Only used on Android.
class SaveCardMessageConfirmController {
 public:
  SaveCardMessageConfirmController(SaveCardMessageConfirmDelegate* delegate,
                                   content::WebContents* web_contents);
  ~SaveCardMessageConfirmController();
  SaveCardMessageConfirmController(const SaveCardMessageConfirmController&) =
      delete;
  SaveCardMessageConfirmController& operator=(
      const SaveCardMessageConfirmController&) = delete;

  void ConfirmSaveCard(const std::u16string& card_label,
                       const std::u16string& cardholder_account);
  void FixName(const std::u16string& inferred_cardholder_name,
               const std::u16string& card_label,
               const std::u16string& cardholder_account);
  void FixDate(const std::u16string& card_label,
               const std::u16string& cardholder_account);

  // Legal Message should be added before `ConfirmSaveCard`, `FixName`
  // and `FixDate` is called.
  void AddLegalMessageLine(const LegalMessageLine& line);

  void DismissDialog();

 private:
  // Returns either the fully initialized java counterpart of this bridge or
  // a is_null() reference if the creation failed. By using this method, the
  // bridge will try to recreate the java object if it failed previously (e.g.
  // because there was no native window available).
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject();

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  raw_ptr<SaveCardMessageConfirmDelegate> delegate_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONFIRM_CONTROLLER_H_
