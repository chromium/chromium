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
#include "bubble_controller_base.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/promos/ios_promos_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

AutofillBubbleBase* ShowSaveBubble(
    const AutofillProfile& profile,
    AutofillClient::SaveAddressBubbleType save_address_bubble_type,
    content::WebContents* web_contents,
    bool shown_by_user_gesture,
    base::WeakPtr<AddressBubbleControllerDelegate> delegate) {
  auto controller = std::make_unique<SaveAddressBubbleController>(
      delegate, web_contents, profile, save_address_bubble_type);

  return BrowserWindow::FindBrowserWindowWithWebContents(web_contents)
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
  return BrowserWindow::FindBrowserWindowWithWebContents(web_contents)
      ->GetAutofillBubbleHandler()
      ->ShowUpdateAddressProfileBubble(
          web_contents, std::move(update_controller), shown_by_user_gesture);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
AutofillBubbleBase* ShowSignInPromo(content::WebContents* web_contents,
                                    const AutofillProfile& autofill_profile) {
  // TODO(crbug.com/381390420): Expose the `AutofillBubbleHandler` in
  // `BrowserWindowInterface` and use that instead.
  return BrowserWindow::FindBrowserWindowWithWebContents(web_contents)
      ->GetAutofillBubbleHandler()
      ->ShowAddressSignInPromo(web_contents, autofill_profile);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

AddressBubblesController::AddressBubblesController(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<AddressBubblesController>(*web_contents),
      app_locale_(g_browser_process->GetFeatures()
                      ->application_locale_storage()
                      ->Get()) {}

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
    AutofillClient::SaveAddressBubbleType save_address_bubble_type,
    bool user_has_any_profile_saved,
    AutofillClient::AddressProfileSavePromptCallback callback) {
  AddressBubblesController::CreateForWebContents(web_contents);
  auto* controller = AddressBubblesController::FromWebContents(web_contents);
  bool is_save_bubble = !original_profile;
  const bool is_migration_to_account =
      save_address_bubble_type ==
      AutofillClient::SaveAddressBubbleType::kMigrateToAccount;
  auto show_bubble_view_impl =
      is_save_bubble
          // Save address bubble.
          ? base::BindRepeating(ShowSaveBubble, profile,
                                save_address_bubble_type)
          // Update address bubble.
          : base::BindRepeating(ShowUpdateBubble, profile, *original_profile);
  std::u16string page_action_icon_tooltip = l10n_util::GetStringUTF16(
      is_save_bubble ? (is_migration_to_account
                            ? IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE
                            : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE)
                     : IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);

  controller->SetUpAndShowBubble(
      std::move(show_bubble_view_impl), std::move(page_action_icon_tooltip),
      is_migration_to_account, user_has_any_profile_saved, std::move(callback));
}

void AddressBubblesController::ShowEditor(
    const AutofillProfile& address_profile,
    const std::u16string& title_override,
    const std::u16string& editor_footer_message,
    bool is_editing_existing_address) {
  DoNotShowNextQueuedBubbleGuard guard = DoNotShowNextQueuedBubble();
  EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents());
  EditAddressProfileDialogControllerImpl* controller =
      EditAddressProfileDialogControllerImpl::FromWebContents(web_contents());
  controller->OfferEdit(
      address_profile, title_override, editor_footer_message,
      is_editing_existing_address, is_migration_to_account_,
      base::BindOnce(&AddressBubblesController::OnUserDecision,
                     weak_ptr_factory_.GetWeakPtr()));
  HideBubble(/*initiated_by_bubble_manager=*/false);
}

void AddressBubblesController::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  if (decision == AutofillClient::AddressPromptUserDecision::kEditDeclined) {
    // Reopen this bubble if the user canceled editing.
    shown_by_user_gesture_ = false;
    QueueOrShowBubble(/*force_show=*/true);
    return;
  }
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_).Run(decision, profile);
    MaybeShowSignInPromo(profile);
  }

  if (decision == AutofillClient::AddressPromptUserDecision::kAccepted ||
      decision == AutofillClient::AddressPromptUserDecision::kEditAccepted) {
    MaybeShowIOSDektopAddressPromo();
  } else if (decision == AutofillClient::AddressPromptUserDecision::kDeclined &&
             !user_has_any_profile_saved_ &&
             base::FeatureList::IsEnabled(
                 features::kAutofillAddressUserDeclinedSaveSurvey)) {
    if (auto* autofill_client =
            ChromeAutofillClient::FromWebContents(web_contents())) {
      autofill_client->TriggerDeclinedSaveAddressReasonSurvey();
    }
  }
}

void AddressBubblesController::OnBubbleClosed() {
  ResetBubbleViewAndInformBubbleManager();
  is_showing_sign_in_promo_ = false;
  UpdatePageActionIcon();
}

void AddressBubblesController::OnIconClicked() {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }
  shown_by_user_gesture_ = true;
  QueueOrShowBubble(/*force_show=*/true);
}

bool AddressBubblesController::IsBubbleActive() const {
  return !address_profile_save_prompt_callback_.is_null() ||
         is_showing_sign_in_promo_;
}

std::u16string AddressBubblesController::GetPageActionIconTooltip() const {
  return page_action_icon_tooltip_;
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

#if !BUILDFLAG(IS_ANDROID)
std::optional<actions::ActionId>
AddressBubblesController::GetActionIdForPageAction() {
  return kActionShowAddressesBubbleOrPage;
}

std::optional<std::u16string>
AddressBubblesController::GetPageActionTooltipText() {
  return GetPageActionIconTooltip();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void AddressBubblesController::DoShowBubble() {
  CHECK(!bubble_view());
  CHECK(show_bubble_view_callback_);

  SetBubbleView(*show_bubble_view_callback_.Run(
      web_contents(), shown_by_user_gesture_, GetWeakPtr()));

  CHECK(bubble_view());
}

BubbleType AddressBubblesController::GetBubbleType() const {
  return BubbleType::kSaveUpdateAddress;
}

base::WeakPtr<BubbleControllerBase>
AddressBubblesController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AddressBubblesController::SetUpAndShowBubble(
    ShowBubbleViewCallback show_bubble_view_callback,
    std::u16string page_action_icon_tooltip,
    bool is_migration_to_account,
    bool user_has_any_profile_saved,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible, and inform the backend.
  if (bubble_view() || !MaySetUpBubble()) {
    std::move(address_profile_save_prompt_callback)
        .Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
             std::nullopt);
    return;
  }

  if (address_profile_save_prompt_callback_) {
    // If the user closed the bubble of the previous import process using the
    // "Close" button without making a decision to "Accept" or "Deny" the
    // prompt, a fallback icon is shown, so the user can get back to the prompt.
    // In this specific scenario the import process is considered in progress
    // (since the backend didn't hear back via the callback yet), but hidden. Or
    // when `bubble_manager_enabled` and the bubble is in the queue to be shown
    // but timed out. When a second prompt arrives, we finish the previous
    // import process as "Ignored", before showing the 2nd prompt.
    std::move(address_profile_save_prompt_callback_)
        .Run(AutofillClient::AddressPromptUserDecision::kIgnored, std::nullopt);
  }

  was_bubble_shown_ = false;

  SetUpBubble(std::move(show_bubble_view_callback),
              std::move(page_action_icon_tooltip), is_migration_to_account,
              user_has_any_profile_saved,
              std::move(address_profile_save_prompt_callback));

  QueueOrShowBubble();
}

void AddressBubblesController::SetUpBubble(
    ShowBubbleViewCallback show_bubble_view_callback,
    std::u16string page_action_icon_tooltip,
    bool is_migration_to_account,
    bool user_has_any_profile_saved,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  show_bubble_view_callback_ = std::move(show_bubble_view_callback);
  page_action_icon_tooltip_ = std::move(page_action_icon_tooltip);
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  shown_by_user_gesture_ = false;
  is_migration_to_account_ = is_migration_to_account;
  user_has_any_profile_saved_ = user_has_any_profile_saved;
}

void AddressBubblesController::MaybeShowIOSDektopAddressPromo() {
  Browser* browser =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents())
          ->AsBrowserView()
          ->browser();

  // Verify if user is eligible for iOS promo, and attempt showing if they are.
  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kAddress, browser);
}

void AddressBubblesController::MaybeShowSignInPromo(
    base::optional_ref<const AutofillProfile> autofill_profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Do nothing if there is no autofill profile or the sign in promo should not
  // be shown.
  if (!autofill_profile.has_value() ||
      !signin::ShouldShowAddressSignInPromo(
          *Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
          autofill_profile.value())) {
    return;
  }

  DoNotShowNextQueuedBubbleGuard guard = DoNotShowNextQueuedBubble();

  // Close the current save bubble.
  HideBubble(/*initiated_by_bubble_manager=*/false);

  // Open the bubble with the sign in promo.
  SetBubbleView(*ShowSignInPromo(web_contents(), autofill_profile.value()));
  CHECK(bubble_view());
  is_showing_sign_in_promo_ = true;
  UpdatePageActionIcon();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AddressBubblesController);

}  // namespace autofill
