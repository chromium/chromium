// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_view.h"

namespace content {
class WebContents;
}

namespace signin {
class IdentityManager;
}

namespace autofill {

class SaveUpdateAddressProfilePromptController;

// JNI wrapper for Java SaveUpdateAddressProfilePrompt.
class SaveUpdateAddressProfilePromptViewAndroid
    : public SaveUpdateAddressProfilePromptView {
 public:
  explicit SaveUpdateAddressProfilePromptViewAndroid(
      content::WebContents* web_contents);
  SaveUpdateAddressProfilePromptViewAndroid(
      const SaveUpdateAddressProfilePromptViewAndroid&) = delete;
  SaveUpdateAddressProfilePromptViewAndroid& operator=(
      const SaveUpdateAddressProfilePromptViewAndroid&) = delete;
  ~SaveUpdateAddressProfilePromptViewAndroid() override;

  // SaveUpdateAddressProfilePromptView:
  bool Show(SaveUpdateAddressProfilePromptController* controller,
            const AutofillProfile& profile,
            bool is_update,
            bool is_migration_to_account) override;

 private:
  // Populates the content of the existing `java_object_` as a save or update
  // prompt (according to `is_update`) with the details supplied by the
  // `controller`.
  void SetContent(SaveUpdateAddressProfilePromptController* controller,
                  signin::IdentityManager* identity_manager,
                  bool is_update);

  // The corresponding Java SaveUpdateAddressProfilePrompt owned by this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_PROMPT_VIEW_ANDROID_H_
