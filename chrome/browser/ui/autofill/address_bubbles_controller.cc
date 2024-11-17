// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/add_new_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/promos/ios_promos_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

AutofillBubbleBase* ShowSaveBubble(
    const AutofillProfile& profile,
    bool is_migration_to_account,
    content::WebContents* web_contents,
    bool shown_by_user_gesture,
    base::WeakPtr<AddressBubbleControllerDelegate> delegate) {
  auto controller = std::make_unique<SaveAddressBubbleController>(
      delegate, web_contents, profile, is_migration_to_account);
  return chrome::FindBrowserWithTab(web_contents)
      ->window()
      ->GetAutofillBubbleHandler()
      ->ShowSaveAddressProfileBubble(web_contents, std::move(controller),
                                     shown_by_user_gesture);
}

AutofillBubbleBase* ShowUpdateBubble(
    const AutofillProfile& profile,
    const AutofillProfile& original_profile,
    content::WebContents* web_contents,
    bool shown_by_user_gesture,
    base::WeakPtr<AddressBubbleControllerDelegate> delegate) {
  auto update_controller = std::make_unique<UpdateAddressBubbleController>(
      delegate, web_contents, profile, original_profile);
  return chrome::FindBrowserWithTab(web_contents)
      ->window()
      ->GetAutofillBubbleHandler()
      ->ShowUpdateAddressProfileBubble(
          web_contents, std::move(update_controller), shown_by_user_gesture);
}

AutofillBubbleBase* ShowAddNewAddressBubble(
    content::WebContents* web_contents,
    bool shown_by_user_gesture,
    base::WeakPtr<AddressBubbleControllerDelegate> delegate) {
  auto controller =
      std::make_unique<AddNewAddressBubbleController>(web_contents, delegate);
  return chrome::FindBrowserWithTab(web_contents)
      ->window()
      ->GetAutofillBubbleHandler()
      ->ShowAddNewAddressProfileBubble(web_contents, std::move(controller),
                                       shown_by_user_gesture);
}
}  // namespace

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

// static
void AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
    content::WebContents* web_contents,
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  AddressBubblesController::CreateForWebContents(web_contents);
  auto* controller = AddressBubblesController::FromWebContents(web_contents);
  bool is_save_bubble = !original_profile;
  auto show_bubble_view_impl =
      is_save_bubble
          // Save address bubble.
          ? base::BindRepeating(ShowSaveBubble, profile,
                                is_migration_to_account)
          // Update address bubble.
          : base::BindRepeating(ShowUpdateBubble, profile, *original_profile);
  std::u16string page_action_icon_tootip = l10n_util::GetStringUTF16(
      is_save_bubble ? (is_migration_to_account
                            ? IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE
                            : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE)
                     : IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);

  controller->SetUpAndShowBubble(std::move(show_bubble_view_impl),
                                 std::move(page_action_icon_tootip),
                                 is_migration_to_account, std::move(callback));
}

// static
void AddressBubblesController::SetUpAndShowAddNewAddressBubble(
    content::WebContents* web_contents,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  AddressBubblesController::CreateForWebContents(web_contents);
  auto* controller = AddressBubblesController::FromWebContents(web_contents);
  std::u16string page_action_icon_tootip =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_NEW_ADDRESS_PROMPT_TITLE);

  controller->SetUpAndShowBubble(base::BindRepeating(ShowAddNewAddressBubble),
                                 std::move(page_action_icon_tootip), {},
                                 std::move(callback));
}

void AddressBubblesController::ShowEditor(
    const AutofillProfile& address_profile,
    const std::u16string& title_override,
    const std::u16string& editor_footer_message,
    bool is_editing_existing_address) {
  EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents());
  EditAddressProfileDialogControllerImpl* controller =
      EditAddressProfileDialogControllerImpl::FromWebContents(web_contents());
  controller->OfferEdit(
      address_profile, title_override, editor_footer_message,
      is_editing_existing_address, is_migration_to_account_,
      base::BindOnce(&AddressBubblesController::OnUserDecision,
                     weak_ptr_factory_.GetWeakPtr()));
  HideBubble();
}

void AddressBubblesController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  if (decision == AutofillClient::AddressPromptUserDecision::kEditDeclined) {
    // Reopen this bubble if the user canceled editing.
    shown_by_user_gesture_ = false;
    Show();
    return;
  }
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_).Run(decision, profile);
  }

// TODO(crbug.com/372209715): Extract out of GOOGLE_CHROME_BRANDING flag.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if ((decision == AutofillClient::AddressPromptUserDecision::kAccepted ||
       decision == AutofillClient::AddressPromptUserDecision::kEditAccepted) &&
      base::FeatureList::IsEnabled(::features::kIOSPromoAddressBubble)) {
    MaybeShowIOSDektopAddressPromo();
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void AddressBubblesController::OnBubbleClosed() {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

void AddressBubblesController::OnIconClicked() {
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
  return page_action_icon_tootip_;
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

  OnUserDecision(AutofillClient::AddressPromptUserDecision::kIgnored,
                 std::nullopt);
}

PageActionIconType AddressBubblesController::GetPageActionIconType() {
  return PageActionIconType::kAutofillAddress;
}

void AddressBubblesController::DoShowBubble() {
  CHECK(!bubble_view());
  CHECK(show_bubble_view_callback_);

  set_bubble_view(show_bubble_view_callback_.Run(
      web_contents(), shown_by_user_gesture_, GetWeakPtr()));

  CHECK(bubble_view());
}

void AddressBubblesController::SetUpAndShowBubble(
    ShowBubbleViewCallback show_bubble_view_callback,
    std::u16string page_action_icon_tootip,
    bool is_migration_to_account,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible, and inform the backend.
  if (bubble_view()) {
    std::move(address_profile_save_prompt_callback)
        .Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
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
        .Run(AutofillClient::AddressPromptUserDecision::kIgnored, std::nullopt);
  }

  show_bubble_view_callback_ = std::move(show_bubble_view_callback);
  page_action_icon_tootip_ = std::move(page_action_icon_tootip);
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  shown_by_user_gesture_ = false;
  is_migration_to_account_ = is_migration_to_account;

  Show();
}

void AddressBubblesController::MaybeShowIOSDektopAddressPromo() {
  // TODO(crbug.com/372209715): Extract out of GOOGLE_CHROME_BRANDING flag.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  Browser* browser = chrome::FindBrowserWithTab(web_contents());

  // Verify if user is eligible for iOS promo, and attempt showing if they are.
  ios_promos_utils::VerifyIOSPromoEligibility(
      IOSPromoType::kAddress, browser->profile(),
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AddressBubblesController);

}  // namespace autofill
