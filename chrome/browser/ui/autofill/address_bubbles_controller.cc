// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AddressBubblesController::AddressBubblesController(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<AddressBubblesController>(*web_contents),
      app_locale_(g_browser_process->GetApplicationLocale()) {}

AddressBubblesController::~AddressBubblesController() {
  // `address_profile_save_prompt_callback_` must have been invoked before
  // destroying the controller to inform the backend of the output of the
  // save/update flow. It's either invoked upon user action when accepting
  // or rejecting the flow, or in cases when users ignore it, it's invoked
  // when the web contents are destroyed.
  DCHECK(address_profile_save_prompt_callback_.is_null());
}

void AddressBubblesController::OfferSave(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::SaveAddressProfilePromptOptions options,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible, and inform the backend.
  if (bubble_view()) {
    std::move(address_profile_save_prompt_callback)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
             std::nullopt);
    return;
  }
  // If the user closed the bubble of the previous import process using the
  // "Close" button without making a decision to "Accept" or "Deny" the prompt,
  // a fallback icon is shown, so the user can get back to the prompt. In this
  // specific scenario the import process is considered in progress (since the
  // backend didn't hear back via the callback yet), but hidden. When a second
  // prompt arrives, we finish the previous import process as "Ignored", before
  // showing the 2nd prompt.
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
             std::nullopt);
  }

  address_profile_ = profile;
  original_profile_ = base::OptionalFromPtr(original_profile);
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  shown_by_user_gesture_ = false;
  is_migration_to_account_ = options.is_migration_to_account;
  if (options.show_prompt) {
    Show();
  }
}

void AddressBubblesController::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  if (decision ==
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditDeclined) {
    // Reopen this bubble if the user canceled editing.
    shown_by_user_gesture_ = false;
    Show();
    return;
  }
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_).Run(decision, profile);
  }
}

void AddressBubblesController::OnEditButtonClicked(
    const std::u16string& editor_footer_message) {
  EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents());
  EditAddressProfileDialogControllerImpl* controller =
      EditAddressProfileDialogControllerImpl::FromWebContents(web_contents());
  controller->OfferEdit(
      *address_profile_, base::OptionalToPtr(original_profile_),
      editor_footer_message,
      base::BindOnce(&AddressBubblesController::OnUserDecision,
                     weak_ptr_factory_.GetWeakPtr()),
      is_migration_to_account_);
  HideBubble();
}

void AddressBubblesController::OnBubbleClosed() {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

void AddressBubblesController::OnPageActionIconClicked() {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }
  shown_by_user_gesture_ = true;
  Show();
}

bool AddressBubblesController::IsBubbleActive() const {
  return !address_profile_save_prompt_callback_.is_null();
}

std::u16string AddressBubblesController::GetPageActionIconTootip() const {
  // TODO(b/325440757): Move tooltip defining outside this class.
  if (IsSaveBubble()) {
    return l10n_util::GetStringUTF16(
        is_migration_to_account_
            ? IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE
            : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

  return l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
}

AutofillBubbleBase* AddressBubblesController::GetBubbleView() const {
  return bubble_view();
}

base::WeakPtr<AddressBubbleControllerDelegate>
AddressBubblesController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AddressBubblesController::WebContentsDestroyed() {
  AutofillBubbleControllerBase::WebContentsDestroyed();

  OnUserDecision(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                 std::nullopt);
}

PageActionIconType AddressBubblesController::GetPageActionIconType() {
  return PageActionIconType::kAutofillAddress;
}

void AddressBubblesController::DoShowBubble() {
  DCHECK(!bubble_view());
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  // TODO(b/325440757): Move the view factory defining outside this class.
  if (IsSaveBubble()) {
    auto save_controller = std::make_unique<SaveAddressBubbleController>(
        GetWeakPtr(), web_contents(), address_profile_.value(),
        is_migration_to_account_);
    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowSaveAddressProfileBubble(
                            web_contents(), std::move(save_controller),
                            shown_by_user_gesture_));
  } else {
    // This is an update prompt.
    auto update_controller = std::make_unique<UpdateAddressBubbleController>(
        GetWeakPtr(), web_contents(), address_profile_.value(),
        original_profile_.value());
    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowUpdateAddressProfileBubble(
                            web_contents(), std::move(update_controller),
                            shown_by_user_gesture_));
  }
  DCHECK(bubble_view());
}

bool AddressBubblesController::IsSaveBubble() const {
  return !original_profile_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AddressBubblesController);

}  // namespace autofill
