// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller_impl.h"

#include <algorithm>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_import_utils.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(crbug.com/441742849): Refactor this class implementation and possibly
// others to remove `chrome::FindBrowserWithTab()`.
namespace autofill {

namespace {

using enum SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType;

bool DidUserDeclineExplicitly(
    SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason
        close_reason) {
  using enum SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason;
  switch (close_reason) {
    case kCancelled:
    case kClosed:
      return true;
    case kAccepted:
    case kUnknown:
    case kNotInteracted:
    case kLostFocus:
      return false;
  }
}

void EmitBubbleFunnelMetrics(
    bool is_save_prompt,
    EntityType entity_type,
    SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason
        close_reason) {
  auto get_save_or_update_histogram_string = [](bool is_save_prompt) {
    return is_save_prompt ? ".SavePrompt" : ".UpdatePrompt";
  };
  auto get_entity_name_for_logging = [](EntityType entity_type) {
    switch (entity_type.name()) {
      case EntityTypeName::kDriversLicense:
        return "DriversLicense";
      case EntityTypeName::kKnownTravelerNumber:
        return "KnownTravelerNumber";
      case EntityTypeName::kNationalIdCard:
        return "NationalIdCard";
      case EntityTypeName::kVehicle:
        return "Vehicle";
      case EntityTypeName::kPassport:
        return "Passport";
      case EntityTypeName::kRedressNumber:
        return "RedressNumber";
      case EntityTypeName::kFlightReservation:
        return "FlightReservation";
    }
    NOTREACHED();
  };
  const std::string prefix = "Autofill.Ai";
  base::UmaHistogramEnumeration(
      base::StrCat({prefix, get_save_or_update_histogram_string(is_save_prompt),
                    ".", get_entity_name_for_logging(entity_type)}),
      close_reason);
  base::UmaHistogramEnumeration(
      base::StrCat({prefix, get_save_or_update_histogram_string(is_save_prompt),
                    ".AllEntities"}),
      close_reason);
}

std::u16string GetPrimaryAccountEmailFromProfile(Profile* profile) {
  if (!profile) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

}  // namespace

SaveOrUpdateAutofillAiDataControllerImpl::
    SaveOrUpdateAutofillAiDataControllerImpl(content::WebContents* web_contents,
                                             const std::string& app_locale)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<SaveOrUpdateAutofillAiDataControllerImpl>(
          *web_contents),
      app_locale_(app_locale) {}

SaveOrUpdateAutofillAiDataControllerImpl::
    ~SaveOrUpdateAutofillAiDataControllerImpl() = default;

// static
SaveOrUpdateAutofillAiDataController*
SaveOrUpdateAutofillAiDataController::GetOrCreate(
    content::WebContents* web_contents,
    const std::string& app_locale) {
  if (!web_contents) {
    return nullptr;
  }

  SaveOrUpdateAutofillAiDataControllerImpl::CreateForWebContents(web_contents,
                                                                 app_locale);
  return SaveOrUpdateAutofillAiDataControllerImpl::FromWebContents(
      web_contents);
}

void SaveOrUpdateAutofillAiDataControllerImpl::ShowPrompt(
    EntityInstance new_entity,
    std::optional<EntityInstance> old_entity,
    AutofillClient::EntitySaveOrUpdatePromptResultCallback
        save_prompt_acceptance_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view() || !MaySetUpBubble()) {
    return;
  }

  SetupPrompt(std::move(new_entity), std::move(old_entity),
              std::move(save_prompt_acceptance_callback));
  QueueOrShowBubble();
}

void SaveOrUpdateAutofillAiDataControllerImpl::SetupPrompt(
    EntityInstance new_entity,
    std::optional<EntityInstance> old_entity,
    AutofillClient::EntitySaveOrUpdatePromptResultCallback
        save_prompt_acceptance_callback) {
  was_bubble_shown_ = false;
  new_entity_ = std::move(new_entity);
  old_entity_ = std::move(old_entity);
  save_prompt_acceptance_callback_ = std::move(save_prompt_acceptance_callback);
}

void SaveOrUpdateAutofillAiDataControllerImpl::OnSaveButtonClicked() {
  OnBubbleClosed(AutofillAiBubbleClosedReason::kAccepted);
}

std::u16string
SaveOrUpdateAutofillAiDataControllerImpl::GetPrimaryAccountEmail() const {
  return GetPrimaryAccountEmailFromProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

bool SaveOrUpdateAutofillAiDataControllerImpl::IsSavePrompt() const {
  return !old_entity_.has_value();
}

std::vector<SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails>
SaveOrUpdateAutofillAiDataControllerImpl::GetUpdatedAttributesDetails() const {
  std::vector<EntityAttributeUpdateDetails> details;

  auto get_attribute_update_type = [&](const AttributeInstance&
                                           new_entity_attribute) {
    if (!old_entity_) {
      return kNewEntityAttributeAdded;
    }

    base::optional_ref<const AttributeInstance> old_entity_attribute =
        old_entity_->attribute(new_entity_attribute.type());
    if (!old_entity_attribute) {
      return kNewEntityAttributeAdded;
    }

    return std::ranges::all_of(new_entity_attribute.type().field_subtypes(),
                               [&](FieldType type) {
                                 return old_entity_attribute->GetInfo(
                                            type, app_locale_,
                                            /*format_string=*/std::nullopt) ==
                                        new_entity_attribute.GetInfo(
                                            type, app_locale_,
                                            /*format_string=*/std::nullopt);
                               })
               ? kNewEntityAttributeUnchanged
               : kNewEntityAttributeUpdated;
  };

  for (const AttributeInstance& attribute : new_entity_->attributes()) {
    EntityAttributeUpdateType update_type =
        get_attribute_update_type(attribute);
    std::u16string attribute_value;
    if (std::optional<std::u16string> date = MaybeGetLocalizedDate(attribute)) {
      attribute_value = *std::move(date);
    } else {
      attribute_value = attribute.GetCompleteInfo(app_locale_);
    }
    if (!attribute_value.empty()) {
      details.emplace_back(attribute.type().GetNameForI18n(),
                           std::move(attribute_value), update_type);
    }
  }

  // Move new entity values that were either added or updated to the top.
  std::ranges::stable_sort(details, [](const EntityAttributeUpdateDetails& a,
                                       const EntityAttributeUpdateDetails& b) {
    // Returns true if `attribute` is a new entity attribute that was either
    // added or updated.
    auto added_or_updated = [](const EntityAttributeUpdateDetails& attribute) {
      return attribute.update_type == kNewEntityAttributeAdded ||
             attribute.update_type == kNewEntityAttributeUpdated;
    };
    if (added_or_updated(a) && !added_or_updated(b)) {
      return true;
    }

    if (!added_or_updated(a) && added_or_updated(b)) {
      return false;
    }
    return false;
  });
  return details;
}

std::u16string SaveOrUpdateAutofillAiDataControllerImpl::GetDialogTitle()
    const {
  if (IsSavePrompt()) {
    switch (new_entity_->type().name()) {
      case EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kKnownTravelerNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kNationalIdCard:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kRedressNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kFlightReservation:
        NOTREACHED() << "Entity is read only and doesn't support save prompts.";
    }
  } else {
    switch (new_entity_->type().name()) {
      case EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kKnownTravelerNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kNationalIdCard:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kRedressNumber:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE);
      case EntityTypeName::kFlightReservation:
        NOTREACHED()
            << "Entity is read only and doesn't support update prompts.";
    }
  }
  NOTREACHED();
}

bool SaveOrUpdateAutofillAiDataControllerImpl::IsWalletableEntity() const {
  return new_entity_->record_type() ==
         EntityInstance::RecordType::kServerWallet;
}

void SaveOrUpdateAutofillAiDataControllerImpl::OnGoToWalletLinkClicked() {
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents())) {
    reopen_bubble_when_web_contents_becomes_visible_ = true;
    ShowSingletonTab(browser, GURL(chrome::kWalletPassesPageURL));
  }
}

void SaveOrUpdateAutofillAiDataControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (IsBubbleManagerEnabled()) {
    // BubbleManager will handle the effects of tab changes.
    return;
  }

  // TODO(crbug.com/441742849): Consider moving this logic to
  // `AutofillBubbleControllerBase`, for now keep it specific to this class to
  // avoid interfeering with other bubbles in transactions.
  AutofillBubbleControllerBase::OnVisibilityChanged(visibility);
  if (visibility == content::Visibility::VISIBLE &&
      reopen_bubble_when_web_contents_becomes_visible_) {
    reopen_bubble_when_web_contents_becomes_visible_ = false;
    QueueOrShowBubble();
  }
}

void SaveOrUpdateAutofillAiDataControllerImpl::OnBubbleClosed(
    SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason
        close_reason) {
  // Make sure competing close calls does not lead to emitting metrics twice.
  if (!bubble_hide_initiated_by_bubble_manager_ && bubble_view()) {
    EmitBubbleFunnelMetrics(IsSavePrompt(), new_entity_->type(), close_reason);
  }
  ResetBubbleViewAndInformBubbleManager();
  UpdatePageActionIcon();

  if (!bubble_hide_initiated_by_bubble_manager_ &&
      !save_prompt_acceptance_callback_.is_null()) {
    std::move(save_prompt_acceptance_callback_)
        .Run({DidUserDeclineExplicitly(close_reason),
              /*entity=*/close_reason == AutofillAiBubbleClosedReason::kAccepted
                  ? std::exchange(new_entity_, std::nullopt)
                  : std::nullopt});
  }
}

void SaveOrUpdateAutofillAiDataControllerImpl::OnBubbleDiscarded() {
  EmitBubbleFunnelMetrics(IsSavePrompt(), new_entity_->type(),
                          was_bubble_shown_
                              ? SaveOrUpdateAutofillAiDataController::
                                    AutofillAiBubbleClosedReason::kNotInteracted
                              : SaveOrUpdateAutofillAiDataController::
                                    AutofillAiBubbleClosedReason::kUnknown);
  if (!save_prompt_acceptance_callback_.is_null()) {
    std::move(save_prompt_acceptance_callback_).Run({false, std::nullopt});
  }
}

std::optional<PageActionIconType>
SaveOrUpdateAutofillAiDataControllerImpl::GetPageActionIconType() {
  return std::nullopt;
}

void SaveOrUpdateAutofillAiDataControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  SetBubbleView(*browser->window()
                     ->GetAutofillBubbleHandler()
                     ->ShowSaveAutofillAiDataBubble(web_contents(), this));
  CHECK(bubble_view());
}

BubbleType SaveOrUpdateAutofillAiDataControllerImpl::GetBubbleType() const {
  return BubbleType::kSaveUpdateAutofillAi;
}

base::WeakPtr<BubbleControllerBase>
SaveOrUpdateAutofillAiDataControllerImpl::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<SaveOrUpdateAutofillAiDataController>
SaveOrUpdateAutofillAiDataControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

int SaveOrUpdateAutofillAiDataControllerImpl::GetTitleImagesResourceId() const {
  switch (new_entity_->type().name()) {
    case EntityTypeName::kDriversLicense:
      return IDR_AUTOFILL_SAVE_DRIVERS_LICENSE_LOTTIE;
    case EntityTypeName::kKnownTravelerNumber:
      return IDR_AUTOFILL_SAVE_KNOWN_TRAVELER_NUMBER_AND_REDRESS_NUMBER_LOTTIE;
    case EntityTypeName::kNationalIdCard:
      return IDR_AUTOFILL_SAVE_PASSPORT_AND_NATIONAL_ID_CARD_LOTTIE;
    case EntityTypeName::kPassport:
      return IDR_AUTOFILL_SAVE_PASSPORT_AND_NATIONAL_ID_CARD_LOTTIE;
    case EntityTypeName::kRedressNumber:
      return IDR_AUTOFILL_SAVE_KNOWN_TRAVELER_NUMBER_AND_REDRESS_NUMBER_LOTTIE;
    case EntityTypeName::kVehicle:
      return IDR_AUTOFILL_SAVE_VEHICLE_LOTTIE;
    case EntityTypeName::kFlightReservation:
      NOTREACHED()
          << "Entity is read only and doesn't support saving/updating.";
  }
  NOTREACHED();
}

base::optional_ref<const EntityInstance>
SaveOrUpdateAutofillAiDataControllerImpl::GetAutofillAiData() const {
  return new_entity_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveOrUpdateAutofillAiDataControllerImpl);

}  // namespace autofill
