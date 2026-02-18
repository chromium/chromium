// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller_impl.h"

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
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"
#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"
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
#include "components/autofill/core/browser/foundations/autofill_client.h"
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

AutofillAiImportDataControllerImpl::AutofillAiImportDataControllerImpl(
    content::WebContents* web_contents,
    const std::string& app_locale)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<AutofillAiImportDataControllerImpl>(
          *web_contents),
      app_locale_(app_locale) {}

AutofillAiImportDataControllerImpl::~AutofillAiImportDataControllerImpl() =
    default;

// static
AutofillAiImportDataController* AutofillAiImportDataController::GetOrCreate(
    content::WebContents* web_contents,
    const std::string& app_locale) {
  if (!web_contents) {
    return nullptr;
  }

  AutofillAiImportDataControllerImpl::CreateForWebContents(web_contents,
                                                           app_locale);
  return AutofillAiImportDataControllerImpl::FromWebContents(web_contents);
}

// static
void AutofillAiImportDataController::Hide(content::WebContents& web_contents) {
  if (auto* controller =
          AutofillAiImportDataControllerImpl::FromWebContents(&web_contents)) {
    controller->HideBubble(/*initiated_by_bubble_manager=*/false);
  }
}

void AutofillAiImportDataControllerImpl::ShowPrompt(
    EntityInstance new_entity,
    std::optional<EntityInstance> old_entity,
    bool close_on_accept,
    AutofillClient::EntityImportPromptResultCallback prompt_result_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view() || !MaySetUpBubble()) {
    if (!prompt_result_callback.is_null()) {
      std::move(prompt_result_callback)
          .Run(AutofillClient::AutofillAiBubbleResult::kUnknown);
    }
    return;
  }

  was_bubble_shown_ = false;
  state_ = SaveUpdateState(std::move(new_entity), std::move(old_entity),
                           close_on_accept, std::move(prompt_result_callback));
  QueueOrShowBubble();
}

void AutofillAiImportDataControllerImpl::ShowLocalSaveNotification() {
  if (bubble_view() || !MaySetUpBubble()) {
    return;
  }

  was_bubble_shown_ = false;
  state_ = LocalSaveNotificationState();
  QueueOrShowBubble();
}

void AutofillAiImportDataControllerImpl::OnSaveButtonClicked() {
  if (GetSaveUpdateState().close_on_accept) {
    OnBubbleClosed(AutofillClient::AutofillAiBubbleResult::kAccepted);
  } else if (!GetSaveUpdateState().prompt_result_callback.is_null()) {
    std::move(GetSaveUpdateState().prompt_result_callback)
        .Run(AutofillClient::AutofillAiBubbleResult::kAccepted);
  }
}

std::u16string AutofillAiImportDataControllerImpl::GetPrimaryAccountEmail()
    const {
  return GetPrimaryAccountEmailFromProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

std::u16string
AutofillAiImportDataControllerImpl::GetSaveUpdateDialogPrimaryButtonText()
    const {
  return GetPrimaryButtonText(IsSavePrompt());
}

bool AutofillAiImportDataControllerImpl::IsSavePrompt() const {
  return !GetSaveUpdateState().old_entity.has_value();
}

std::vector<EntityAttributeUpdateDetails>
AutofillAiImportDataControllerImpl::GetUpdatedAttributesDetails() const {
  return EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
      GetSaveUpdateState().new_entity, GetSaveUpdateState().old_entity,
      app_locale_);
}

std::u16string AutofillAiImportDataControllerImpl::GetSaveUpdateDialogTitle()
    const {
  return GetPromptTitle(GetSaveUpdateState().new_entity.type().name(),
                        IsSavePrompt());
}

bool AutofillAiImportDataControllerImpl::IsWalletableEntity() const {
  return GetSaveUpdateState().new_entity.record_type() ==
         EntityInstance::RecordType::kServerWallet;
}

void AutofillAiImportDataControllerImpl::OnGoToWalletLinkClicked() {
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents())) {
    reopen_bubble_when_web_contents_becomes_visible_ = true;
    ShowSingletonTab(browser, GURL(chrome::kWalletPassesPageURL));
  }
}

void AutofillAiImportDataControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (IsBubbleManagerEnabled()) {
    // BubbleManager will handle the effects of tab changes.
    return;
  }

  // TODO(crbug.com/441742849): Consider moving this logic to
  // `AutofillBubbleControllerBase`, for now keep it specific to this class to
  // avoid interfering with other bubbles in transactions.
  AutofillBubbleControllerBase::OnVisibilityChanged(visibility);
  if (visibility == content::Visibility::VISIBLE &&
      reopen_bubble_when_web_contents_becomes_visible_) {
    QueueOrShowBubble();
  }
}

bool AutofillAiImportDataControllerImpl::ShouldReshowOnTabVisible() const {
  return reopen_bubble_when_web_contents_becomes_visible_;
}

void AutofillAiImportDataControllerImpl::OnBubbleClosed(
    AutofillClient::AutofillAiBubbleResult result) {
  ResetBubbleViewAndInformBubbleManager();
  UpdatePageActionIcon();

  if (!bubble_hide_initiated_by_bubble_manager_) {
    MaybeRunSaveUpdateCallback(result);
  }
}

bool AutofillAiImportDataControllerImpl::CanBeReshown() const {
  // We reshow the prompt only if it is a save/update prompt that has not run
  // yet. The other cases offer too little benefit to the user.
  return IsSaveUpdatePrompt() && GetSaveUpdateState().prompt_result_callback;
}

void AutofillAiImportDataControllerImpl::OnBubbleDiscarded() {
  using enum AutofillClient::AutofillAiBubbleResult;
  MaybeRunSaveUpdateCallback(was_bubble_shown_ ? kNotInteracted : kUnknown);
}

std::optional<PageActionIconType>
AutofillAiImportDataControllerImpl::GetPageActionIconType() {
  return std::nullopt;
}

void AutofillAiImportDataControllerImpl::DoShowBubble() {
  auto get_bubble = [this]() -> AutofillBubbleBase& {
    Browser* browser = chrome::FindBrowserWithTab(web_contents());
    if (IsSaveUpdatePrompt()) {
      return *browser->window()
          ->GetAutofillBubbleHandler()
          ->ShowSaveAutofillAiDataBubble(web_contents(), this);
    }
    if (IsLocalSaveNotification()) {
      return *browser->window()
                  ->GetAutofillBubbleHandler()
                  ->ShowAutofillAiLocalSaveNotification(web_contents(), this);
    }
    NOTREACHED();
  };

  reopen_bubble_when_web_contents_becomes_visible_ = false;
  SetBubbleView(get_bubble());
  CHECK(bubble_view());
}

BubbleType AutofillAiImportDataControllerImpl::GetBubbleType() const {
  return BubbleType::kSaveUpdateAutofillAi;
}

base::WeakPtr<BubbleControllerBase>
AutofillAiImportDataControllerImpl::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<AutofillAiImportDataController>
AutofillAiImportDataControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

int AutofillAiImportDataControllerImpl::
    GetSaveUpdateDialogTitleImagesResourceId() const {
  switch (GetSaveUpdateState().new_entity.type().name()) {
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
    case EntityTypeName::kOrder:
      NOTREACHED()
          << "Entity is read only and doesn't support saving/updating.";
  }
  NOTREACHED();
}

base::optional_ref<const EntityInstance>
AutofillAiImportDataControllerImpl::GetAutofillAiData() const {
  return GetSaveUpdateState().new_entity;
}

bool AutofillAiImportDataControllerImpl::CloseOnAccept() const {
  return GetSaveUpdateState().close_on_accept;
}

void AutofillAiImportDataControllerImpl::MaybeRunSaveUpdateCallback(
    AutofillClient::AutofillAiBubbleResult result) {
  if (IsSaveUpdatePrompt() &&
      !GetSaveUpdateState().prompt_result_callback.is_null()) {
    std::move(GetSaveUpdateState().prompt_result_callback).Run(result);
  }
}

AutofillAiImportDataControllerImpl::SaveUpdateState::SaveUpdateState(
    EntityInstance new_entity,
    std::optional<EntityInstance> old_entity,
    bool close_on_accept,
    AutofillClient::EntityImportPromptResultCallback prompt_result_callback)
    : new_entity(std::move(new_entity)),
      old_entity(std::move(old_entity)),
      close_on_accept(close_on_accept),
      prompt_result_callback(std::move(prompt_result_callback)) {}

AutofillAiImportDataControllerImpl::SaveUpdateState::SaveUpdateState(
    SaveUpdateState&&) = default;

AutofillAiImportDataControllerImpl::SaveUpdateState&
AutofillAiImportDataControllerImpl::SaveUpdateState::operator=(
    SaveUpdateState&&) = default;

AutofillAiImportDataControllerImpl::SaveUpdateState::~SaveUpdateState() =
    default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutofillAiImportDataControllerImpl);

}  // namespace autofill
