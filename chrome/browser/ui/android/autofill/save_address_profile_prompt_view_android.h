// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_ADDRESS_PROFILE_PROMPT_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_ADDRESS_PROFILE_PROMPT_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/autofill/android/save_address_profile_prompt_view.h"

namespace content {
class WebContents;
}

namespace autofill {

class SaveAddressProfilePromptController;

// JNI wrapper for Java SaveAddressProfilePrompt.
class SaveAddressProfilePromptViewAndroid
    : public SaveAddressProfilePromptView {
 public:
  explicit SaveAddressProfilePromptViewAndroid(
      content::WebContents* web_contents);
  SaveAddressProfilePromptViewAndroid(
      const SaveAddressProfilePromptViewAndroid&) = delete;
  SaveAddressProfilePromptViewAndroid& operator=(
      const SaveAddressProfilePromptViewAndroid&) = delete;
  ~SaveAddressProfilePromptViewAndroid() override;

  // SaveAddressProfilePromptView:
  bool Show(SaveAddressProfilePromptController* controller,
            const AutofillProfile& profile,
            bool is_update) override;

 private:
  // Populates the content of the existing `java_object_` as a save or update
  // prompt (according to `is_update`) with the details supplied by the
  // `controller`.
  void SetContent(SaveAddressProfilePromptController* controller,
                  bool is_update);

  // The corresponding Java SaveAddressProfilePrompt owned by this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  content::WebContents* web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_ADDRESS_PROFILE_PROMPT_VIEW_ANDROID_H_
