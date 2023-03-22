// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/autofill/android/save_update_address_profile_message_controller.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Class to manage save new/update existing address profile flow on Android. The
// flow consists of 2 steps: showing a confirmation message and a prompt with
// profile details to review. This class owns and triggers the corresponding
// controllers.
class SaveUpdateAddressProfileFlowManager {
 public:
  SaveUpdateAddressProfileFlowManager();
  SaveUpdateAddressProfileFlowManager(
      const SaveUpdateAddressProfileFlowManager&) = delete;
  SaveUpdateAddressProfileFlowManager& operator=(
      const SaveUpdateAddressProfileFlowManager&) = delete;
  ~SaveUpdateAddressProfileFlowManager();

  // Triggers a confirmation flow for saving the `profile` using the given
  // `web_contents`. If another flow is in progress, the incoming offer will
  // be auto-declined. The `original_profile` is nullptr for a new address or
  // points to the existing profile which will be updated if the user accepts.
  // User will be offered to migrate their address profile to their Google
  // Account when `is_migration_to_account` is true. The `callback` is triggered
  // once the user makes a decision.
  void OfferSave(content::WebContents* web_contents,
                 const AutofillProfile& profile,
                 const AutofillProfile* original_profile,
                 bool is_migration_to_account,
                 AutofillClient::AddressProfileSavePromptCallback callback);

  SaveUpdateAddressProfileMessageController* GetMessageControllerForTest();
  SaveUpdateAddressProfilePromptController* GetPromptControllerForTest();

 private:
  void ShowConfirmationMessage(
      content::WebContents* web_contents,
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback callback);

  void ShowPromptWithDetails(
      content::WebContents* web_contents,
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback callback);

  void OnPromptWithDetailsDismissed();

  SaveUpdateAddressProfileMessageController
      save_update_address_profile_message_controller_;
  std::unique_ptr<SaveUpdateAddressProfilePromptController>
      save_update_address_profile_prompt_controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_H_
