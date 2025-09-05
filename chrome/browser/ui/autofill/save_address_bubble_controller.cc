// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace autofill {

SaveAddressBubbleController::SaveAddressBubbleController(
    base::WeakPtr<AddressBubbleControllerDelegate> delegate,
    content::WebContents* web_contents,
    const AutofillProfile& address_profile,
    AutofillClient::SaveAddressBubbleType save_address_bubble_type)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate),
      address_profile_(address_profile),
      save_address_bubble_type_(save_address_bubble_type) {}

SaveAddressBubbleController::~SaveAddressBubbleController() = default;

std::u16string SaveAddressBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16([this] {
    switch (save_address_bubble_type_) {
      case AutofillClient::SaveAddressBubbleType::kSave:
        return IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE;
      case autofill::AutofillClient::SaveAddressBubbleType::kMigrateToAccount:
        return IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE;
      case autofill::AutofillClient::SaveAddressBubbleType::
          kHomeWorkNameEmailMerge:
        return IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_NAME_EMAIL_HOME_WORK_MERGE_PROMPT_TITLE;
    }
  }());
}

std::optional<SaveAddressBubbleController::HeaderImages>
SaveAddressBubbleController::GetHeaderImages() const {
  if (IsMigrationToAccount() && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());
    if (account) {
      // The position and size must match the implied one in the image,
      // so these numbers are exclusively for ..._AVATAR50_X135_Y54.
      static constexpr gfx::Point kAvatarPosition{135, 54};
      static constexpr size_t kAvatarSize{50};
      return HeaderImages{
          .light = profiles::EmbedAvatarOntoImage(
              IDR_MIGRATE_ADDRESS_AVATAR50_X135_Y54, account->account_image,
              kAvatarPosition, kAvatarSize),
          .dark = profiles::EmbedAvatarOntoImage(
              IDR_MIGRATE_ADDRESS_AVATAR50_X135_Y54_DARK,
              account->account_image, kAvatarPosition, kAvatarSize)};
    }
  }

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  return HeaderImages{.lottie = bundle.GetThemedLottieImageNamed(
                          IDR_AUTOFILL_SAVE_ADDRESS_LOTTIE)};
}

std::u16string SaveAddressBubbleController::GetBodyText() const {
  if (IsMigrationToAccount() && web_contents()) {
    PersonalDataManager& pdm =
        ContentAutofillClient::FromWebContents(web_contents())
            ->GetPersonalDataManager();

    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    int string_id = pdm.address_data_manager().IsSyncFeatureEnabledForAutofill()
                        ? IDS_AUTOFILL_SYNCABLE_PROFILE_MIGRATION_PROMPT_NOTICE
                        : IDS_AUTOFILL_LOCAL_PROFILE_MIGRATION_PROMPT_NOTICE;

    return l10n_util::GetStringFUTF16(string_id,
                                      base::UTF8ToUTF16(account->email));
  }

  return {};
}

std::u16string SaveAddressBubbleController::GetAddressSummary() const {
  // Use a shorter version of the address summary for migration, it has
  // a fixed set of fields and doesn't depend on libaddressinput.
  if (IsMigrationToAccount()) {
    static constexpr std::array fields = {
        NAME_FULL, ADDRESS_HOME_LINE1, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER};
    std::vector<std::u16string> values;
    for (FieldType field : fields) {
      std::u16string value =
          address_profile_.GetInfo(field, g_browser_process->GetFeatures()
                                              ->application_locale_storage()
                                              ->Get());
      if (!value.empty()) {
        values.push_back(value);
      }
    }
    if (values.empty()) {
      return {};
    }
    return base::JoinString(values, u"\n");
  }

  return GetEnvelopeStyleAddress(
      address_profile_,
      g_browser_process->GetFeatures()->application_locale_storage()->Get(),
      /*include_recipient=*/true, /*include_country=*/true);
}

std::u16string SaveAddressBubbleController::GetProfileEmail() const {
  // Email is not shown as a separate field in the migration flow,
  // it is included in the address summary, see GetAddressSummary().
  if (IsMigrationToAccount()) {
    return {};
  }

  return address_profile_.GetInfo(
      EMAIL_ADDRESS,
      g_browser_process->GetFeatures()->application_locale_storage()->Get());
}

std::u16string SaveAddressBubbleController::GetProfilePhone() const {
  // Phone is not shown as a separate field in the migration flow,
  // it is included in the address summary, see GetAddressSummary().
  if (IsMigrationToAccount()) {
    return {};
  }

  return autofill::i18n::GetFormattedPhoneNumberForDisplay(
      address_profile_,
      g_browser_process->GetFeatures()->application_locale_storage()->Get());
}

std::u16string SaveAddressBubbleController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16([this] {
    switch (save_address_bubble_type_) {
      case AutofillClient::SaveAddressBubbleType::kSave:
        return IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE;
      case autofill::AutofillClient::SaveAddressBubbleType::kMigrateToAccount:
        return IDS_AUTOFILL_MIGRATE_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE;
      case autofill::AutofillClient::SaveAddressBubbleType::
          kHomeWorkNameEmailMerge:
        return IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_NAME_EMAIL_HOME_WORK_MERGE_OK_BUTTON_LABEL;
    }
  }());
}

std::u16string SaveAddressBubbleController::GetNegativeButtonLabel() const {
  return l10n_util::GetStringUTF16([this] {
    switch (save_address_bubble_type_) {
      case AutofillClient::SaveAddressBubbleType::kSave:
      case autofill::AutofillClient::SaveAddressBubbleType::kMigrateToAccount:
        return IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_CANCEL_BUTTON_LABEL;
      case autofill::AutofillClient::SaveAddressBubbleType::
          kHomeWorkNameEmailMerge:
        return IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_NAME_EMAIL_HOME_WORK_MERGE_CANCEL_BUTTON_LABEL;
    }
  }());
}

AutofillClient::AddressPromptUserDecision
SaveAddressBubbleController::GetCancelCallbackValue() const {
  // The migration prompt should not be shown again if the user explicitly
  // rejects it (for a particular address, due to legal and privacy
  // requirements). In other cases it is acceptable to show it a few times more.
  return IsMigrationToAccount()
             ? AutofillClient::AddressPromptUserDecision::kNever
             : AutofillClient::AddressPromptUserDecision::kDeclined;
}

std::u16string SaveAddressBubbleController::GetFooterMessage() const {
  if (address_profile_.IsAccountProfile() && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());

    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return {};
}

std::u16string SaveAddressBubbleController::GetEditorFooterMessage() const {
  if (IsMigrationToAccount() && web_contents()) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents()->GetBrowserContext());
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_SAVE_IN_ACCOUNT_PROMPT_ADDRESS_SOURCE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  return GetFooterMessage();
}

void SaveAddressBubbleController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  if (delegate_) {
    delegate_->OnUserDecision(decision, profile);
  }
}

void SaveAddressBubbleController::OnEditButtonClicked() {
  if (delegate_) {
    delegate_->ShowEditor(address_profile_, /*title_override=*/u"",
                          GetEditorFooterMessage(),
                          /*is_editing_existing_address=*/false);
  }
}

void SaveAddressBubbleController::OnBubbleClosed() {
  if (delegate_) {
    delegate_->OnBubbleClosed();
  }
}

bool SaveAddressBubbleController::IsMigrationToAccount() const {
  return save_address_bubble_type_ ==
         AutofillClient::SaveAddressBubbleType::kMigrateToAccount;
}

}  // namespace autofill
