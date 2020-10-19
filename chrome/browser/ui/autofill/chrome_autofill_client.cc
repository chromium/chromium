// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "chrome/browser/autofill/autofill_offer_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_controller.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/autofill_log_router_factory.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/webauthn/internal_authenticator_impl.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/android/signin/signin_promo_util_android.h"
#include "chrome/browser/autofill/android/internal_authenticator_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#include "chrome/browser/ui/android/autofill/card_expiration_date_fix_flow_view_android.h"
#include "chrome/browser/ui/android/autofill/card_name_fix_flow_view_android.h"
#include "chrome/browser/ui/android/infobars/autofill_credit_card_filling_infobar.h"
#include "components/autofill/core/browser/payments/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"
#include "components/infobars/core/infobar.h"
#include "ui/android/window_android.h"
#else  // !OS_ANDROID
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_upi_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/zoom/zoom_controller.h"
#endif

namespace autofill {

using AutoselectFirstSuggestion =
    AutofillClient::PopupOpenArgs::AutoselectFirstSuggestion;

ChromeAutofillClient::~ChromeAutofillClient() {
  // NOTE: It is too late to clean up the autofill popup; that cleanup process
  // requires that the WebContents instance still be valid and it is not at
  // this point (in particular, the WebContentsImpl destructor has already
  // finished running and we are now in the base class destructor).
  DCHECK(!popup_controller_);
}

version_info::Channel ChromeAutofillClient::GetChannel() const {
  return chrome::GetChannel();
}

PersonalDataManager* ChromeAutofillClient::GetPersonalDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return PersonalDataManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

AutocompleteHistoryManager*
ChromeAutofillClient::GetAutocompleteHistoryManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return AutocompleteHistoryManagerFactory::GetForProfile(profile);
}

PrefService* ChromeAutofillClient::GetPrefs() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext())
      ->GetPrefs();
}

syncer::SyncService* ChromeAutofillClient::GetSyncService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return ProfileSyncServiceFactory::GetForProfile(profile);
}

signin::IdentityManager* ChromeAutofillClient::GetIdentityManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
}

FormDataImporter* ChromeAutofillClient::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* ChromeAutofillClient::GetPaymentsClient() {
  return payments_client_.get();
}

StrikeDatabase* ChromeAutofillClient::GetStrikeDatabase() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  // No need to return a StrikeDatabase in incognito mode. It is primarily
  // used to determine whether or not to offer save of Autofill data. However,
  // we don't allow saving of Autofill data while in incognito anyway, so an
  // incognito code path should never get far enough to query StrikeDatabase.
  return StrikeDatabaseFactory::GetForProfile(profile);
}

ukm::UkmRecorder* ChromeAutofillClient::GetUkmRecorder() {
  return ukm::UkmRecorder::Get();
}

ukm::SourceId ChromeAutofillClient::GetUkmSourceId() {
  return ukm::GetSourceIdForWebContentsDocument(web_contents());
}

AddressNormalizer* ChromeAutofillClient::GetAddressNormalizer() {
  return AddressNormalizerFactory::GetInstance();
}

AutofillOfferManager* ChromeAutofillClient::GetAutofillOfferManager() {
  return AutofillOfferManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

const GURL& ChromeAutofillClient::GetLastCommittedURL() {
  return web_contents()->GetLastCommittedURL();
}

security_state::SecurityLevel
ChromeAutofillClient::GetSecurityLevelForUmaHistograms() {
  SecurityStateTabHelper* helper =
      ::SecurityStateTabHelper::FromWebContents(web_contents());

  // If there is no helper, it means we are not in a "web" state (for example
  // the file picker on CrOS). Return SECURITY_LEVEL_COUNT which will not be
  // logged.
  if (!helper)
    return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;

  return helper->GetSecurityLevel();
}

const translate::LanguageState* ChromeAutofillClient::GetLanguageState() {
  // TODO(crbug.com/912597): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents());
  if (translate_manager)
    return translate_manager->GetLanguageState();
  return nullptr;
}

std::string ChromeAutofillClient::GetVariationConfigCountryCode() const {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  // Retrieves the country code from variation service and converts it to upper
  // case.
  return variation_service
             ? base::ToUpperASCII(variation_service->GetLatestCountry())
             : std::string();
}

std::unique_ptr<InternalAuthenticator>
ChromeAutofillClient::CreateCreditCardInternalAuthenticator(
    content::RenderFrameHost* rfh) {
#if defined(OS_ANDROID)
  return std::make_unique<InternalAuthenticatorAndroid>(rfh);
#else
  return std::make_unique<content::InternalAuthenticatorImpl>(rfh);
#endif
}

void ChromeAutofillClient::ShowAutofillSettings(
    bool show_credit_card_settings) {
#if defined(OS_ANDROID)
  if (show_credit_card_settings) {
    ShowAutofillCreditCardSettings(web_contents());
  } else {
    ShowAutofillProfileSettings(web_contents());
  }
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    if (show_credit_card_settings) {
      chrome::ShowSettingsSubPage(browser, chrome::kPaymentsSubPage);
    } else {
      chrome::ShowSettingsSubPage(browser, chrome::kAddressesSubPage);
    }
  }
#endif  // #if defined(OS_ANDROID)
}

void ChromeAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      base::BindOnce(&CreateCardUnmaskPromptView,
                     base::Unretained(&unmask_controller_),
                     base::Unretained(web_contents())),
      card, reason, delegate);
}

void ChromeAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

#if !defined(OS_ANDROID)
std::vector<std::string>
ChromeAutofillClient::GetAllowedMerchantsForVirtualCards() {
  if (!prefs::IsAutofillCreditCardEnabled(GetPrefs()))
    return std::vector<std::string>();

  return AutofillGstaticReader::GetInstance()
      ->GetTokenizationMerchantWhitelist();
}

std::vector<std::string>
ChromeAutofillClient::GetAllowedBinRangesForVirtualCards() {
  if (!prefs::IsAutofillCreditCardEnabled(GetPrefs()))
    return std::vector<std::string>();

  return AutofillGstaticReader::GetInstance()
      ->GetTokenizationBinRangesWhitelist();
}

void ChromeAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  ManageMigrationUiController::CreateForWebContents(web_contents());
  ManageMigrationUiController* controller =
      ManageMigrationUiController::FromWebContents(web_contents());
  controller->ShowBubble(std::move(show_migration_dialog_closure));
}

void ChromeAutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  ManageMigrationUiController::CreateForWebContents(web_contents());
  ManageMigrationUiController* controller =
      ManageMigrationUiController::FromWebContents(web_contents());
  controller->ShowOfferDialog(legal_message_lines, user_email,
                              migratable_credit_cards,
                              std::move(start_migrating_cards_callback));
}

void ChromeAutofillClient::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const base::string16& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {
  ManageMigrationUiController::CreateForWebContents(web_contents());
  ManageMigrationUiController* controller =
      ManageMigrationUiController::FromWebContents(web_contents());
  controller->UpdateCreditCardIcon(has_server_error, tip_message,
                                   migratable_credit_cards,
                                   delete_local_card_callback);
}

void ChromeAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {
  WebauthnDialogControllerImpl::CreateForWebContents(web_contents());
  WebauthnDialogControllerImpl::FromWebContents(web_contents())
      ->ShowOfferDialog(std::move(offer_dialog_callback));
}

void ChromeAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {
  WebauthnDialogControllerImpl::CreateForWebContents(web_contents());
  WebauthnDialogControllerImpl::FromWebContents(web_contents())
      ->ShowVerifyPendingDialog(std::move(verify_pending_dialog_callback));
}

void ChromeAutofillClient::UpdateWebauthnOfferDialogWithError() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::FromWebContents(web_contents());
  if (controller)
    controller->UpdateDialog(WebauthnDialogState::kOfferError);
}

bool ChromeAutofillClient::CloseWebauthnDialog() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::FromWebContents(web_contents());
  if (controller)
    return controller->CloseDialog();

  return false;
}

void ChromeAutofillClient::ConfirmSaveUpiIdLocally(
    const std::string& upi_id,
    base::OnceCallback<void(bool accept)> callback) {
  SaveUPIBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveUPIBubbleControllerImpl* controller =
      SaveUPIBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferUpiIdLocalSave(upi_id, std::move(callback));
}

void ChromeAutofillClient::OfferVirtualCardOptions(
    const std::vector<CreditCard*>& candidates,
    base::OnceCallback<void(const std::string&)> callback) {
  VirtualCardSelectionDialogControllerImpl::CreateForWebContents(
      web_contents());
  VirtualCardSelectionDialogControllerImpl::FromWebContents(web_contents())
      ->ShowDialog(candidates, std::move(callback));
}

#else  // defined(OS_ANDROID)
void ChromeAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const base::string16&)> callback) {
  CardNameFixFlowViewAndroid* card_name_fix_flow_view_android =
      new CardNameFixFlowViewAndroid(&card_name_fix_flow_controller_,
                                     web_contents());
  card_name_fix_flow_controller_.Show(
      card_name_fix_flow_view_android, GetAccountHolderName(),
      /*upload_save_card_callback=*/std::move(callback));
}

void ChromeAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const base::string16&, const base::string16&)>
        callback) {
  CardExpirationDateFixFlowViewAndroid*
      card_expiration_date_fix_flow_view_android =
          new CardExpirationDateFixFlowViewAndroid(
              &card_expiration_date_fix_flow_controller_, web_contents());
  card_expiration_date_fix_flow_controller_.Show(
      card_expiration_date_fix_flow_view_android, card,
      /*upload_save_card_callback=*/std::move(callback));
}
#endif

void ChromeAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
#if defined(OS_ANDROID)
  DCHECK(options.show_prompt);
  InfoBarService::FromWebContents(web_contents())
      ->AddInfoBar(CreateSaveCardInfoBarMobile(
          std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
              /*upload=*/false, options, card, LegalMessageLines(),
              /*upload_save_card_callback=*/
              AutofillClient::UploadSaveCardPromptCallback(),
              /*local_save_card_callback=*/std::move(callback), GetPrefs())));
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferLocalSave(card, options, std::move(callback));
#endif
}

void ChromeAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
#if defined(OS_ANDROID)
  DCHECK(options.show_prompt);
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
      save_card_info_bar_delegate_mobile =
          std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
              /*upload=*/true, options, card, legal_message_lines,
              /*upload_save_card_callback=*/std::move(callback),
              /*local_save_card_callback=*/
              AutofillClient::LocalSaveCardPromptCallback(), GetPrefs());
  InfoBarService::FromWebContents(web_contents())
      ->AddInfoBar(CreateSaveCardInfoBarMobile(
          std::move(save_card_info_bar_delegate_mobile)));
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferUploadSave(card, legal_message_lines, options,
                              std::move(callback));
#endif
}

void ChromeAutofillClient::CreditCardUploadCompleted(bool card_saved) {
#if defined(OS_ANDROID)
  // TODO(siyua@): Placeholder for Clank Notification.
#else
  if (!base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback)) {
    return;
  }

  // Do lazy initialization of SaveCardBubbleControllerImpl.
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  card_saved ? controller->UpdateIconForSaveCardSuccess()
             : controller->UpdateIconForSaveCardFailure();
#endif
}

void ChromeAutofillClient::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {
#if defined(OS_ANDROID)
  auto infobar_delegate =
      std::make_unique<AutofillCreditCardFillingInfoBarDelegateMobile>(
          card, std::move(callback));
  auto* raw_delegate = infobar_delegate.get();
  if (InfoBarService::FromWebContents(web_contents())
          ->AddInfoBar(std::make_unique<AutofillCreditCardFillingInfoBar>(
              std::move(infobar_delegate)))) {
    raw_delegate->set_was_shown();
  }
#endif
}

bool ChromeAutofillClient::HasCreditCardScanFeature() {
  return CreditCardScannerController::HasCreditCardScanFeature();
}

void ChromeAutofillClient::ScanCreditCard(CreditCardScanCallback callback) {
  CreditCardScannerController::ScanCreditCard(web_contents(),
                                              std::move(callback));
}

void ChromeAutofillClient::ShowAutofillPopup(
    const autofill::AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      open_args.element_bounds + client_area.OffsetFromOrigin();

  // Will delete or reuse the old |popup_controller_|.
  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_, delegate, web_contents(),
      web_contents()->GetNativeView(), element_bounds_in_screen_space,
      open_args.text_direction);

  popup_controller_->Show(open_args.suggestions,
                          open_args.autoselect_first_suggestion.value(),
                          open_args.popup_type);
}

void ChromeAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  if (popup_controller_.get())
    popup_controller_->UpdateDataListValues(values, labels);
}

base::span<const Suggestion> ChromeAutofillClient::GetPopupSuggestions() const {
  if (!popup_controller_.get())
    return base::span<const Suggestion>();
  return popup_controller_->GetUnelidedSuggestions();
}

void ChromeAutofillClient::PinPopupView() {
  if (popup_controller_.get())
    popup_controller_->PinView();
}

autofill::AutofillClient::PopupOpenArgs
ChromeAutofillClient::GetReopenPopupArgs() const {
  const AutofillPopupController* controller = popup_controller_.get();
  if (!controller)
    return autofill::AutofillClient::PopupOpenArgs();

  // By calculating the screen space-independent values, bounds can be passed to
  // |ShowAutofillPopup| which always computes the bounds in the screen space.
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  gfx::RectF screen_space_independent_bounds =
      controller->element_bounds() - client_area.OffsetFromOrigin();
  return autofill::AutofillClient::PopupOpenArgs(
      screen_space_independent_bounds,
      controller->IsRTL() ? base::i18n::RIGHT_TO_LEFT
                          : base::i18n::LEFT_TO_RIGHT,
      controller->GetSuggestions(), AutoselectFirstSuggestion(false),
      controller->GetPopupType());
}

void ChromeAutofillClient::UpdatePopup(
    const std::vector<Suggestion>& suggestions,
    PopupType popup_type) {
  if (!popup_controller_.get())
    return;  // Update only if there is a popup.

  // When a form changes dynamically, |popup_controller_| may hold a delegate of
  // the wrong type, so updating the popup would call into the wrong delegate.
  // Hence, just close the existing popup (crbug/1113241).
  // The cast is needed to access AutofillPopupController::GetPopupType().
  if (popup_type !=
      static_cast<const AutofillPopupController*>(popup_controller_.get())
          ->GetPopupType()) {
    popup_controller_->Hide(PopupHidingReason::kStaleData);
    return;
  }

  // Calling show will reuse the existing view automatically
  popup_controller_->Show(suggestions, /*autoselect_first_suggestion=*/false,
                          popup_type);
}

void ChromeAutofillClient::HideAutofillPopup(PopupHidingReason reason) {
  if (popup_controller_.get())
    popup_controller_->Hide(reason);
}

bool ChromeAutofillClient::IsAutocompleteEnabled() {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

void ChromeAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  if (driver) {
    driver->GetPasswordGenerationHelper()->ProcessPasswordRequirements(forms);
    driver->GetPasswordManager()->ProcessAutofillPredictions(driver, forms);
  }
}

void ChromeAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
#if defined(OS_ANDROID)
  AutofillLoggerAndroid::DidFillOrPreviewField(autofilled_value,
                                               profile_full_name);
#endif  // defined(OS_ANDROID)
}

bool ChromeAutofillClient::IsContextSecure() {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  if (!helper)
    return false;

  const auto security_level = helper->GetSecurityLevel();
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  // Only dangerous security states should prevent autofill.
  //
  // TODO(crbug.com/701018): Once passive mixed content and legacy TLS are less
  // common, just use IsSslCertificateValid().
  return entry && entry->GetURL().SchemeIsCryptographic() &&
         security_level != security_state::DANGEROUS;
}

bool ChromeAutofillClient::ShouldShowSigninPromo() {
#if !defined(OS_ANDROID)
  return false;
#else
  return signin::ShouldShowPromo(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
#endif
}

bool ChromeAutofillClient::AreServerCardsSupported() {
  // When in VR, server side cards are not supported.
  return !vr::VrTabHelper::IsInVr(web_contents());
}

void ChromeAutofillClient::ExecuteCommand(int id) {
#if defined(OS_ANDROID)
  if (id == POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO) {
    auto* window = web_contents()->GetNativeView()->GetWindowAndroid();
    if (window) {
      chrome::android::SigninPromoUtilAndroid::StartSigninActivityForPromo(
          window, signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
    }
  }
#endif
}

LogManager* ChromeAutofillClient::GetLogManager() const {
  return log_manager_.get();
}

void ChromeAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  risk_util::LoadRiskData(0, web_contents(), std::move(callback));
}

void ChromeAutofillClient::MainFrameWasResized(bool width_changed) {
#if defined(OS_ANDROID)
  // Ignore virtual keyboard showing and hiding a strip of suggestions.
  if (!width_changed)
    return;
#endif

  HideAutofillPopup(PopupHidingReason::kWidgetChanged);
}

void ChromeAutofillClient::WebContentsDestroyed() {
  HideAutofillPopup(PopupHidingReason::kTabGone);
}

#if !defined(OS_ANDROID)
void ChromeAutofillClient::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  HideAutofillPopup(PopupHidingReason::kContentAreaMoved);
}
#endif  // !defined(OS_ANDROID)

ChromeAutofillClient::ChromeAutofillClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      payments_client_(std::make_unique<payments::PaymentsClient>(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetURLLoaderFactory(),
          GetIdentityManager(),
          GetPersonalDataManager(),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->IsOffTheRecord())),
      form_data_importer_(std::make_unique<FormDataImporter>(
          this,
          payments_client_.get(),
          GetPersonalDataManager(),
          GetPersonalDataManager()->app_locale())),
      unmask_controller_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())) {
  // TODO(crbug.com/928595): Replace the closure with a callback to the renderer
  // that indicates if log messages should be sent from the renderer.
  log_manager_ =
      LogManager::Create(AutofillLogRouterFactory::GetForBrowserContext(
                             web_contents->GetBrowserContext()),
                         base::Closure());
  // Initialize StrikeDatabase so its cache will be loaded and ready to use when
  // when requested by other Autofill classes.
  GetStrikeDatabase();

#if !defined(OS_ANDROID)
  // Since ZoomController is also a WebContentsObserver, we need to be careful
  // about disconnecting from it since the relative order of destruction of
  // WebContentsObservers is not guaranteed. ZoomController silently clears
  // its ZoomObserver list during WebContentsDestroyed() so there's no need
  // to explicitly remove ourselves on destruction.
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  // There may not always be a ZoomController, e.g. in tests.
  if (zoom_controller)
    zoom_controller->AddObserver(this);
#endif
}

Profile* ChromeAutofillClient::GetProfile() const {
  if (!web_contents())
    return nullptr;
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

base::string16 ChromeAutofillClient::GetAccountHolderName() {
  Profile* profile = GetProfile();
  if (!profile)
    return base::string16();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return base::string16();
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager->GetPrimaryAccountInfo());
  return primary_account_info
             ? base::UTF8ToUTF16(primary_account_info->full_name)
             : base::string16();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeAutofillClient)

}  // namespace autofill
