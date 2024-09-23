// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/add_new_address_bubble_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

AddressCountryCode GetCountryCodeForNewAddress(
    content::WebContents* web_contents) {
  PersonalDataManager* pdm =
      ContentAutofillClient::FromWebContents(web_contents)
          ->GetPersonalDataManager();
  return pdm->address_data_manager().GetDefaultCountryCodeForNewAddress();
}

bool IsEligibleForAccountStorage(content::WebContents* web_contents,
                                 const std::string& country_code) {
  PersonalDataManager* pdm =
      ContentAutofillClient::FromWebContents(web_contents)
          ->GetPersonalDataManager();

  // Note: addresses from unsupported countries can't be saved in account.
  // TODO(crbug.com/40263955): remove temporary unsupported countries
  // filtering.
  return pdm->address_data_manager().IsEligibleForAddressAccountStorage() &&
         pdm->address_data_manager().IsCountryEligibleForAccountStorage(
             country_code);
}

}  // namespace

AddNewAddressBubbleController::AddNewAddressBubbleController(
    content::WebContents* web_contents,
    base::WeakPtr<AddressBubbleControllerDelegate> delegate)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      country_code_(GetCountryCodeForNewAddress(web_contents)),
      is_eligible_for_account_storage_(
          IsEligibleForAccountStorage(web_contents, country_code_.value())) {}

AddNewAddressBubbleController::~AddNewAddressBubbleController() = default;

std::u16string AddNewAddressBubbleController::GetBodyText() const {
  return l10n_util::GetStringUTF16(
      is_eligible_for_account_storage_
          ? IDS_AUTOFILL_ADD_NEW_ADDRESS_INTO_ACCOUNT_PROMPT_BODY_TEXT
          : IDS_AUTOFILL_ADD_NEW_ADDRESS_INTO_CHROME_PROMPT_BODY_TEXT);
}

std::u16string AddNewAddressBubbleController::GetFooterMessage() const {
  if (is_eligible_for_account_storage_ && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return {};
}

void AddNewAddressBubbleController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision) {
  if (delegate_) {
    delegate_->OnUserDecision(decision, std::nullopt);
  }
}

void AddNewAddressBubbleController::OnAddButtonClicked() {
  if (delegate_) {
    delegate_->ShowEditor(
        AutofillProfile(is_eligible_for_account_storage_
                            ? AutofillProfile::RecordType::kAccount
                            : AutofillProfile::RecordType::kLocalOrSyncable,
                        country_code_),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_NEW_ADDRESS_EDITOR_TITLE),
        GetFooterMessage(),
        /*is_editing_existing_address=*/false);
  }
}

void AddNewAddressBubbleController::OnBubbleClosed() {
  if (delegate_) {
    delegate_->OnBubbleClosed();
  }
}

}  // namespace autofill
