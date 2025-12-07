// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"

#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

UpdateAddressBubbleController::UpdateAddressBubbleController(
    base::WeakPtr<AddressBubbleControllerDelegate> delegate,
    content::WebContents* web_contents,
    const AutofillProfile& profile_to_save,
    const AutofillProfile& original_profile)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      profile_to_save_(profile_to_save),
      original_profile_(original_profile) {}

UpdateAddressBubbleController::~UpdateAddressBubbleController() = default;

std::u16string UpdateAddressBubbleController::GetWindowTitle(
    bool has_empty_original_values) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForHomeAndWork)) {
    // A new profile is created when extending a Home & Work profile, so the
    // title should reflect this.
    if (original_profile_.IsHomeAndWorkProfile()) {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_ADDRESS_WITH_MORE_INFO_ADDRESS_PROMPT_TITLE);
    }
    // If there are no old values to replace, inform user that a new data
    // point is being added to the profile.
    if (has_empty_original_values) {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_ADD_NEW_INFO_ADDRESS_PROMPT_TITLE);
    }
  }

  return l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
}

std::u16string UpdateAddressBubbleController::GetFooterMessage() const {
  if (profile_to_save_.IsAccountProfile() && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForHomeAndWork)) {
      return l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE,
          base::UTF8ToUTF16(account->email));
    }

    switch (original_profile_.record_type()) {
      case AutofillProfile::RecordType::kAccountHome:
        return l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_ADDRESS_HOME_RECORD_TYPE_NOTICE,
            base::UTF8ToUTF16(account->email));
      case AutofillProfile::RecordType::kAccountWork:
        return l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_ADDRESS_WORK_RECORD_TYPE_NOTICE,
            base::UTF8ToUTF16(account->email));
      case AutofillProfile::RecordType::kAccount:
        return l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE,
            base::UTF8ToUTF16(account->email));
      case AutofillProfile::RecordType::kAccountNameEmail:
        NOTIMPLEMENTED();
        break;
      case AutofillProfile::RecordType::kLocalOrSyncable:
        NOTREACHED();
    }
  }

  return {};
}

std::u16string UpdateAddressBubbleController::GetPositiveButtonText(
    bool has_empty_original_values) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForHomeAndWork)) {
    if (original_profile_.IsHomeAndWorkProfile()) {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
    }
    if (has_empty_original_values) {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_UPDATE_ADDRESS_ADD_NEW_INFO_PROMPT_OK_BUTTON_LABEL);
    }
  }

  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
}

std::u16string UpdateAddressBubbleController::GetNegativeButtonText(
    bool has_empty_original_values) const {
  if ((original_profile_.IsHomeAndWorkProfile() || has_empty_original_values) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForHomeAndWork)) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_UPDATE_ADDRESS_ADD_NEW_INFO_PROMPT_CANCEL_BUTTON_LABEL);
  }

  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL);
}

const AutofillProfile& UpdateAddressBubbleController::GetProfileToSave() const {
  return profile_to_save_;
}

const AutofillProfile& UpdateAddressBubbleController::GetOriginalProfile()
    const {
  return original_profile_;
}

void UpdateAddressBubbleController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  if (delegate_) {
    delegate_->OnUserDecision(decision, profile);
  }
}

void UpdateAddressBubbleController::OnEditButtonClicked() {
  if (delegate_) {
    delegate_->ShowEditor(profile_to_save_, /*title_override=*/u"",
                          GetFooterMessage(),
                          /*is_editing_existing_address=*/true);
  }
}

void UpdateAddressBubbleController::OnBubbleClosed() {
  if (delegate_) {
    delegate_->OnBubbleClosed();
  }
}
}  // namespace autofill
