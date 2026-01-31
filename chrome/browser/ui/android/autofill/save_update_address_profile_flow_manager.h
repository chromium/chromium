// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_controller.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_view.h"
#include "chrome/browser/ui/autofill/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class ContentAutofillClient;

// Class to manage save new/update existing address profile flow on Android. The
// flow consists of 2 steps: showing a confirmation message and a prompt with
// profile details to review. This class owns and triggers the corresponding
// controllers.
class SaveUpdateAddressProfileFlowManager {
 public:
  static constexpr int kMessageDescriptionMaxLines = 2;

  SaveUpdateAddressProfileFlowManager(
      ContentAutofillClient* owner,
      AutofillMessageController* autofill_message_controller);
  SaveUpdateAddressProfileFlowManager(
      const SaveUpdateAddressProfileFlowManager&) = delete;
  SaveUpdateAddressProfileFlowManager& operator=(
      const SaveUpdateAddressProfileFlowManager&) = delete;
  virtual ~SaveUpdateAddressProfileFlowManager();

  // Triggers a confirmation flow for saving the `profile` using the given
  // `web_contents`. If another flow is in progress, the incoming offer will
  // be auto-declined. The `original_profile` is nullptr for a new address or
  // points to the existing profile which will be updated if the user accepts.
  // User will be offered to migrate their address profile to their Google
  // Account depending on the `prompt_mode`. The `callback` is triggered once
  // the user makes a decision.
  void OfferSave(const AutofillProfile& profile,
                 const AutofillProfile* original_profile,
                 SaveUpdateAddressProfilePromptMode prompt_mode,
                 AutofillClient::AddressProfileSavePromptCallback callback);

 protected:
  virtual std::unique_ptr<SaveUpdateAddressProfilePromptView>
  CreatePromptView();

 private:
  friend class SaveUpdateAddressProfileFlowManagerTestApi;

  void ShowConfirmationMessage(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveUpdateAddressProfilePromptMode prompt_mode,
      AutofillClient::AddressProfileSavePromptCallback callback);

  void OnMessagePrimaryAction();

  void OnMessageDismissed(messages::DismissReason dismiss_reason);

  void RunSaveAddressProfileCallback(
      AutofillClient::AddressPromptUserDecision decision);

  std::unique_ptr<AutofillMessageModel> CreateMessageModel();

  std::u16string GetMessageTitle() const;

  std::u16string GetMessageDescription() const;

  std::u16string GetMessageRecordTypeNotice() const;

  std::u16string GetMessagePrimaryButtonText() const;

  void ShowPromptWithDetails(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveUpdateAddressProfilePromptMode prompt_mode,
      AutofillClient::AddressProfileSavePromptCallback callback);

  void OnPromptWithDetailsDismissed();

  bool IsMigrationToAccount() const;

  const raw_ref<ContentAutofillClient> owner_;
  raw_ref<AutofillMessageController> autofill_message_controller_;
  std::unique_ptr<SaveUpdateAddressProfilePromptController>
      save_update_address_profile_prompt_controller_;
  std::unique_ptr<messages::MessageWrapper> message_;

  bool is_message_displayed_ = false;
  // The mode the message is displayed in.
  SaveUpdateAddressProfilePromptMode prompt_mode_;
  // The profile which is being confirmed by the user.
  std::optional<AutofillProfile> profile_;
  // The profile (if exists) which will be updated if the user confirms.
  raw_ptr<const AutofillProfile> original_profile_ = nullptr;
  // The callback to run once the user makes the final decision.
  AutofillClient::AddressProfileSavePromptCallback
      save_address_profile_callback_;

  base::WeakPtrFactory<SaveUpdateAddressProfileFlowManager> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_FLOW_MANAGER_H_
