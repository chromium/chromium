// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller_impl.h"

#include <algorithm>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_import_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_ai {

namespace {

using autofill::AutofillAiDelegate;
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
    autofill::EntityType entity_type,
    SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason
        close_reason) {
  auto get_save_or_update_histogram_string = [](bool is_save_prompt) {
    return is_save_prompt ? ".SavePrompt" : ".UpdatePrompt";
  };
  auto get_entity_name_for_logging = [](autofill::EntityType entity_type) {
    switch (entity_type.name()) {
      case autofill::EntityTypeName::kVehicle:
        return "Vehicle";
      case autofill::EntityTypeName::kPassport:
        return "Passport";
      case autofill::EntityTypeName::kDriversLicense:
        return "DriversLicense";
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
    autofill::EntityInstance new_entity,
    std::optional<autofill::EntityInstance> old_entity,
    AutofillAiClient::SaveOrUpdatePromptResultCallback
        save_prompt_acceptance_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }
  new_entity_ = std::move(new_entity);
  old_entity_ = std::move(old_entity);
  save_prompt_acceptance_callback_ = std::move(save_prompt_acceptance_callback);
  DoShowBubble();
}

void SaveOrUpdateAutofillAiDataControllerImpl::OnSaveButtonClicked() {
  OnBubbleClosed(AutofillAiBubbleClosedReason::kAccepted);
}

bool SaveOrUpdateAutofillAiDataControllerImpl::IsSavePrompt() const {
  return !old_entity_.has_value();
}

std::vector<SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails>
SaveOrUpdateAutofillAiDataControllerImpl::GetUpdatedAttributesDetails() const {
  std::vector<EntityAttributeUpdateDetails> details;

  auto get_attribute_update_type = [&](const autofill::AttributeInstance&
                                           new_entity_attribute) {
    if (!old_entity_) {
      return kNewEntityAttributeAdded;
    }

    base::optional_ref<const autofill::AttributeInstance> old_entity_attribute =
        old_entity_->attribute(new_entity_attribute.type());
    if (!old_entity_attribute) {
      return kNewEntityAttributeAdded;
    }

    return std::ranges::all_of(new_entity_attribute.GetSupportedTypes(),
                               [&](autofill::FieldType type) {
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

  for (const autofill::AttributeInstance& attribute :
       new_entity_->attributes()) {
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
      case autofill::EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE);
      case autofill::EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE);
      case autofill::EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
    }
  } else {
    switch (new_entity_->type().name()) {
      case autofill::EntityTypeName::kVehicle:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE);
      case autofill::EntityTypeName::kPassport:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE);
      case autofill::EntityTypeName::kDriversLicense:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
    }
  }
  NOTREACHED();
}

void SaveOrUpdateAutofillAiDataControllerImpl::OnBubbleClosed(
    SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason
        close_reason) {
  // Make sure competing close calls does not lead to emitting metrics twice.
  if (bubble_view()) {
    EmitBubbleFunnelMetrics(IsSavePrompt(), new_entity_->type(), close_reason);
  }
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
  if (!save_prompt_acceptance_callback_.is_null()) {
    std::move(save_prompt_acceptance_callback_)
        .Run({DidUserDeclineExplicitly(close_reason),
              /*entity=*/close_reason == AutofillAiBubbleClosedReason::kAccepted
                  ? std::exchange(new_entity_, std::nullopt)
                  : std::nullopt});
  }
}

PageActionIconType
SaveOrUpdateAutofillAiDataControllerImpl::GetPageActionIconType() {
  // TODO(crbug.com/362227379): Update icon.
  return PageActionIconType::kAutofillAddress;
}

void SaveOrUpdateAutofillAiDataControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowSaveAutofillAiDataBubble(web_contents(), this));
  CHECK(bubble_view());
}

base::WeakPtr<SaveOrUpdateAutofillAiDataController>
SaveOrUpdateAutofillAiDataControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// TODO(389629676): Return proper resources once we have them.
std::pair<int, int>
SaveOrUpdateAutofillAiDataControllerImpl::GetTitleImagesResourceId() const {
  switch (new_entity_->type().name()) {
    case autofill::EntityTypeName::kVehicle:
      return {IDR_SAVE_VEHICLE, IDR_SAVE_VEHICLE_DARK};
    case autofill::EntityTypeName::kPassport:
      return {IDR_SAVE_PASSPORT, IDR_SAVE_PASSPORT_DARK};
    case autofill::EntityTypeName::kDriversLicense:
      return {IDR_SAVE_DRIVERS_LICENSE, IDR_SAVE_DRIVERS_LICENSE_DARK};
  }
  NOTREACHED();
}

base::optional_ref<const autofill::EntityInstance>
SaveOrUpdateAutofillAiDataControllerImpl::GetAutofillAiData() const {
  return new_entity_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveOrUpdateAutofillAiDataControllerImpl);

}  // namespace autofill_ai
