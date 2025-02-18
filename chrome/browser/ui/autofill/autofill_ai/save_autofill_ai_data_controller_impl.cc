// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/save_autofill_ai_data_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/integrators/autofill_ai_delegate.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_ai {

namespace {

using autofill::AutofillAiDelegate;

// Returns whether user interacted with the bubble, based on its closed reason.
bool GetUserInteractionFromAutofillAiBubbleClosedReason(
    SaveAutofillAiDataController::AutofillAiBubbleClosedReason closed_reason) {
  using enum SaveAutofillAiDataController::AutofillAiBubbleClosedReason;
  switch (closed_reason) {
    case kAccepted:
    case kCancelled:
    case kClosed:
      return true;
    case kUnknown:
    case kNotInteracted:
    case kLostFocus:
      return false;
  }
}

}  // namespace

SaveAutofillAiDataControllerImpl::SaveAutofillAiDataControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<SaveAutofillAiDataControllerImpl>(
          *web_contents) {}

SaveAutofillAiDataControllerImpl::~SaveAutofillAiDataControllerImpl() = default;

// static
SaveAutofillAiDataController* SaveAutofillAiDataController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SaveAutofillAiDataControllerImpl::CreateForWebContents(web_contents);
  return SaveAutofillAiDataControllerImpl::FromWebContents(web_contents);
}

void SaveAutofillAiDataControllerImpl::OfferSave(
    autofill::EntityInstance new_entity,
    std::optional<autofill::EntityInstance> old_entity,
    AutofillAiClient::SavePromptAcceptanceCallback
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

void SaveAutofillAiDataControllerImpl::OnSaveButtonClicked() {
  OnBubbleClosed(AutofillAiBubbleClosedReason::kAccepted);
}

std::u16string SaveAutofillAiDataControllerImpl::GetDialogTitle() const {
  const bool is_update = old_entity_.has_value();
  if (!is_update) {
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

      case autofill::EntityTypeName::kLoyaltyCard:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_SAVE_LOYALTY_CARD_ENTITY_DIALOG_TITLE);
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

      case autofill::EntityTypeName::kLoyaltyCard:
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AI_UPDATE_LOYALTY_CARD_ENTITY_DIALOG_TITLE);
    }
  }

  NOTREACHED();
}

void SaveAutofillAiDataControllerImpl::OnBubbleClosed(
    SaveAutofillAiDataController::AutofillAiBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
  if (!save_prompt_acceptance_callback_.is_null()) {
    std::move(save_prompt_acceptance_callback_)
        .Run(
            {/*did_user_interact=*/
             GetUserInteractionFromAutofillAiBubbleClosedReason(closed_reason),
             /*entity=*/closed_reason == AutofillAiBubbleClosedReason::kAccepted
                 ? std::exchange(new_entity_, std::nullopt)
                 : std::nullopt});
  }
}

PageActionIconType SaveAutofillAiDataControllerImpl::GetPageActionIconType() {
  // TODO(crbug.com/362227379): Update icon.
  return PageActionIconType::kAutofillAddress;
}

void SaveAutofillAiDataControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowSaveAutofillAiDataBubble(web_contents(), this));
  CHECK(bubble_view());
}

base::WeakPtr<SaveAutofillAiDataController>
SaveAutofillAiDataControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::optional_ref<const autofill::EntityInstance>
SaveAutofillAiDataControllerImpl::GetAutofillAiData() const {
  return new_entity_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveAutofillAiDataControllerImpl);

}  // namespace autofill_ai
