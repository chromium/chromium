// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "chrome/browser/autofill/autofill_offer_manager_factory.h"
#include "chrome/browser/autofill/iban_manager_factory.h"
#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_controller.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/autofill_log_router_factory.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"
#include "components/webauthn/content/browser/internal_authenticator_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/preferences/autofill/autofill_profile_bridge.h"
#include "chrome/browser/android/signin/signin_bridge.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view_impl.h"
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#include "chrome/browser/ui/android/autofill/card_expiration_date_fix_flow_view_android.h"
#include "chrome/browser/ui/android/autofill/card_name_fix_flow_view_android.h"
#include "chrome/browser/ui/android/infobars/autofill_credit_card_filling_infobar.h"
#include "chrome/browser/ui/android/infobars/autofill_offer_notification_infobar.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_controller_android.h"
#include "components/autofill/core/browser/payments/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/messages/android/messages_feature.h"
#include "components/webauthn/android/internal_authenticator_android.h"
#include "ui/android/window_android.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_upi_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/zoom/zoom_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

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

IBANManager* ChromeAutofillClient::GetIBANManager() {
  if (!base::FeatureList::IsEnabled(features::kAutofillFillIbanFields))
    return nullptr;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return IBANManagerFactory::GetForProfile(profile);
}

MerchantPromoCodeManager* ChromeAutofillClient::GetMerchantPromoCodeManager() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillFillMerchantPromoCodeFields)) {
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return MerchantPromoCodeManagerFactory::GetForProfile(profile);
}

CreditCardCVCAuthenticator* ChromeAutofillClient::GetCVCAuthenticator() {
  if (!cvc_authenticator_)
    cvc_authenticator_ = std::make_unique<CreditCardCVCAuthenticator>(this);
  return cvc_authenticator_.get();
}

CreditCardOtpAuthenticator* ChromeAutofillClient::GetOtpAuthenticator() {
  if (!otp_authenticator_)
    otp_authenticator_ = std::make_unique<CreditCardOtpAuthenticator>(this);
  return otp_authenticator_.get();
}

PrefService* ChromeAutofillClient::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* ChromeAutofillClient::GetPrefs() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext())
      ->GetPrefs();
}

syncer::SyncService* ChromeAutofillClient::GetSyncService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return SyncServiceFactory::GetForProfile(profile);
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
  return web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

AddressNormalizer* ChromeAutofillClient::GetAddressNormalizer() {
  return AddressNormalizerFactory::GetInstance();
}

AutofillOfferManager* ChromeAutofillClient::GetAutofillOfferManager() {
  return AutofillOfferManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

const GURL& ChromeAutofillClient::GetLastCommittedPrimaryMainFrameURL() const {
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL();
}

url::Origin ChromeAutofillClient::GetLastCommittedPrimaryMainFrameOrigin()
    const {
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
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
  // TODO(crbug.com/912597): iOS vs other platforms extracts the language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents());
  if (translate_manager)
    return translate_manager->GetLanguageState();
  return nullptr;
}

translate::TranslateDriver* ChromeAutofillClient::GetTranslateDriver() {
  // TODO(crbug.com/912597): iOS vs other platforms extracts the language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  if (translate_client)
    return translate_client->translate_driver();
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

profile_metrics::BrowserProfileType ChromeAutofillClient::GetProfileType()
    const {
  Profile* profile = GetProfile();
  // Profile can only be null in tests, therefore it is safe to always return
  // |kRegular| when it does not exist.
  return profile ? profile_metrics::GetBrowserProfileType(profile)
                 : profile_metrics::BrowserProfileType::kRegular;
}

std::unique_ptr<webauthn::InternalAuthenticator>
ChromeAutofillClient::CreateCreditCardInternalAuthenticator(
    AutofillDriver* driver) {
  auto* cad = static_cast<ContentAutofillDriver*>(driver);
  content::RenderFrameHost* rfh = cad->render_frame_host();
  if (!rfh)
    return nullptr;
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<InternalAuthenticatorAndroid>(rfh);
#else
  return std::make_unique<content::InternalAuthenticatorImpl>(rfh);
#endif
}

void ChromeAutofillClient::ShowAutofillSettings(
    bool show_credit_card_settings) {
#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeAutofillClient::ShowCardUnmaskOtpInputDialog(
    const size_t& otp_length,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  CardUnmaskOtpInputDialogControllerImpl::CreateForWebContents(web_contents());
  CardUnmaskOtpInputDialogControllerImpl* controller =
      CardUnmaskOtpInputDialogControllerImpl::FromWebContents(web_contents());
  DCHECK(controller);
  controller->ShowDialog(otp_length, delegate);
}

void ChromeAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  CardUnmaskOtpInputDialogControllerImpl::CreateForWebContents(web_contents());
  CardUnmaskOtpInputDialogControllerImpl* controller =
      CardUnmaskOtpInputDialogControllerImpl::FromWebContents(web_contents());
  DCHECK(controller);
  controller->OnOtpVerificationResult(unmask_result);
}

void ChromeAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
        base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      base::BindOnce(&CreateCardUnmaskPromptView,
                     base::Unretained(&unmask_controller_),
                     base::Unretained(web_contents())),
      card, card_unmask_prompt_options, delegate);
}

// TODO(crbug.com/1220990): Refactor this for both CVC and Biometrics flows.
void ChromeAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
#if BUILDFLAG(IS_ANDROID)
  // For VCN-related errors, on Android we show a new error dialog instead of
  // updating the CVC unmask prompt with the error message.
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ShowVirtualCardErrorDialog(
          AutofillErrorDialogContext::WithPermanentOrTemporaryError(
              /*is_permanent_error=*/true));
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ShowVirtualCardErrorDialog(
          AutofillErrorDialogContext::WithPermanentOrTemporaryError(
              /*is_permanent_error=*/false));
      break;
    case AutofillClient::PaymentsRpcResult::kSuccess:
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      // Do nothing
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeAutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  CardUnmaskAuthenticationSelectionDialogControllerImpl::GetOrCreate(
      web_contents())
      ->ShowDialog(challenge_options,
                   std::move(confirm_unmask_challenge_option_callback),
                   std::move(cancel_unmasking_closure));
}

void ChromeAutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {
  CardUnmaskAuthenticationSelectionDialogControllerImpl::GetOrCreate(
      web_contents())
      ->DismissDialogUponServerProcessedAuthenticationMethodRequest(
          server_success);
}

VirtualCardEnrollmentManager*
ChromeAutofillClient::GetVirtualCardEnrollmentManager() {
  return form_data_importer_->GetVirtualCardEnrollmentManager();
}

void ChromeAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());
  DCHECK(controller);
  controller->ShowBubble(virtual_card_enrollment_fields,
                         std::move(accept_virtual_card_callback),
                         std::move(decline_virtual_card_callback));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void ChromeAutofillClient::HideVirtualCardEnrollBubbleAndIconIfVisible() {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());

  if (controller && controller->IsIconVisible())
    controller->HideIconAndBubble();
}
#endif

#if !BUILDFLAG(IS_ANDROID)
std::vector<std::string>
ChromeAutofillClient::GetAllowedMerchantsForVirtualCards() {
  if (!prefs::IsAutofillCreditCardEnabled(GetPrefs()))
    return std::vector<std::string>();

  return AutofillGstaticReader::GetInstance()
      ->GetTokenizationMerchantAllowlist();
}

std::vector<std::string>
ChromeAutofillClient::GetAllowedBinRangesForVirtualCards() {
  if (!prefs::IsAutofillCreditCardEnabled(GetPrefs()))
    return std::vector<std::string>();

  return AutofillGstaticReader::GetInstance()
      ->GetTokenizationBinRangesAllowlist();
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
    const std::u16string& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {
  ManageMigrationUiController::CreateForWebContents(web_contents());
  ManageMigrationUiController* controller =
      ManageMigrationUiController::FromWebContents(web_contents());
  controller->UpdateCreditCardIcon(has_server_error, tip_message,
                                   migratable_credit_cards,
                                   delete_local_card_callback);
}

void ChromeAutofillClient::ConfirmSaveIBANLocally(
    const IBAN& iban,
    bool should_show_prompt,
    LocalSaveIBANPromptCallback callback) {
  NOTIMPLEMENTED();
  // TODO(crbug.com/1349109): Implement SaveIBANBubbleController to show
  // prompt bubble for local save.
}

void ChromeAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {
  WebauthnDialogControllerImpl::GetOrCreateForPage(
      web_contents()->GetPrimaryPage())
      ->ShowOfferDialog(std::move(offer_dialog_callback));
}

void ChromeAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {
  WebauthnDialogControllerImpl::GetOrCreateForPage(
      web_contents()->GetPrimaryPage())
      ->ShowVerifyPendingDialog(std::move(verify_pending_dialog_callback));
}

void ChromeAutofillClient::UpdateWebauthnOfferDialogWithError() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::GetForPage(
          web_contents()->GetPrimaryPage());
  if (controller)
    controller->UpdateDialog(WebauthnDialogState::kOfferError);
}

bool ChromeAutofillClient::CloseWebauthnDialog() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::GetForPage(
          web_contents()->GetPrimaryPage());
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

#else  // BUILDFLAG(IS_ANDROID)
void ChromeAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  CardNameFixFlowViewAndroid* card_name_fix_flow_view_android =
      new CardNameFixFlowViewAndroid(&card_name_fix_flow_controller_,
                                     web_contents());
  card_name_fix_flow_controller_.Show(
      card_name_fix_flow_view_android, GetAccountHolderName(),
      /*upload_save_card_callback=*/std::move(callback));
}

void ChromeAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
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
#if BUILDFLAG(IS_ANDROID)
  DCHECK(options.show_prompt);
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateSaveCardInfoBarMobile(
          std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
              /*upload=*/false, options, card, LegalMessageLines(),
              AutofillClient::UploadSaveCardPromptCallback(),
              /*local_save_card_callback=*/std::move(callback),
              AccountInfo())));
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
#if BUILDFLAG(IS_ANDROID)
  DCHECK(options.show_prompt);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateSaveCardInfoBarMobile(
          std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
              /*upload=*/true, options, card, legal_message_lines,
              /*upload_save_card_callback=*/std::move(callback),
              AutofillClient::LocalSaveCardPromptCallback(), account_info)));
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferUploadSave(card, legal_message_lines, options,
                              std::move(callback));
#endif
}

void ChromeAutofillClient::CreditCardUploadCompleted(bool card_saved) {}

void ChromeAutofillClient::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {
#if BUILDFLAG(IS_ANDROID)
  auto infobar_delegate =
      std::make_unique<AutofillCreditCardFillingInfoBarDelegateMobile>(
          card, std::move(callback));
  auto* raw_delegate = infobar_delegate.get();
  if (infobars::ContentInfoBarManager::FromWebContents(web_contents())
          ->AddInfoBar(std::make_unique<AutofillCreditCardFillingInfoBar>(
              std::move(infobar_delegate)))) {
    raw_delegate->set_was_shown();
  }
#endif
}

void ChromeAutofillClient::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveAddressProfilePromptOptions options,
    AddressProfileSavePromptCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1167061): Respect SaveAddressProfilePromptOptions.
  save_update_address_profile_flow_manager_.OfferSave(
      web_contents(), profile, original_profile, std::move(callback));
#else
  SaveUpdateAddressProfileBubbleControllerImpl::CreateForWebContents(
      web_contents());
  SaveUpdateAddressProfileBubbleControllerImpl* controller =
      SaveUpdateAddressProfileBubbleControllerImpl::FromWebContents(
          web_contents());
  controller->OfferSave(profile, original_profile, options,
                        std::move(callback));
#endif
}

bool ChromeAutofillClient::HasCreditCardScanFeature() {
  return CreditCardScannerController::HasCreditCardScanFeature();
}

void ChromeAutofillClient::ScanCreditCard(CreditCardScanCallback callback) {
  CreditCardScannerController::ScanCreditCard(web_contents(),
                                              std::move(callback));
}

bool ChromeAutofillClient::IsFastCheckoutSupported() {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(::features::kFastCheckout)) {
    LOG_AF(log_manager_.get())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because FastCheckout flag is disabled.";
    return false;
  }

  // Not supported if MakeSearchesAndBrowsingBetter is not enabled. This has
  // been done to allow for consequent hash dances during consent-less flows.
  if (!GetPrefs()->GetBoolean(
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled)) {
    LOG_AF(log_manager_.get())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because the client is not MSBB.";
    return false;
  }

  if (!GetPersonalDataManager()->IsAutofillProfileEnabled()) {
    LOG_AF(log_manager_.get())
        << LoggingScope::kFastCheckout << autofill::LogMessage::kFastCheckout
        << "not triggered because Autofill profile is disabled.";
    return false;
  }

  if (!GetPersonalDataManager()->IsAutofillCreditCardEnabled()) {
    LOG_AF(log_manager_.get())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "if disabled, not triggered Autofill credit card is disabled.";
    return false;
  }

  // Not supported on CCTs.
  auto* tab_android = TabAndroid::FromWebContents(web_contents());
  if (tab_android && tab_android->IsCustomTab()) {
    LOG_AF(log_manager_.get())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because the tab is CCT.";
    return false;
  }

  return true;
#else
  return false;
#endif
}

bool ChromeAutofillClient::IsFastCheckoutTriggerForm(
    const FormData& form,
    const FormFieldData& field) {
#if BUILDFLAG(IS_ANDROID)
  FastCheckoutCapabilitiesFetcher* fetcher =
      FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
          GetProfile());
  if (!fetcher) {
    return false;
  }
  // TODO(crbug.com/1356498): Stop calculating the signature once the form
  // signature has been moved to `form_data`.
  // Check browser form's signature and renderer form's signature.
  FormSignature form_signature = CalculateFormSignature(form);
  bool is_trigger_form =
      fetcher->IsTriggerFormSupported(form.main_frame_origin, form_signature) ||
      fetcher->IsTriggerFormSupported(form.main_frame_origin,
                                      field.host_form_signature);
  if (!is_trigger_form) {
    LOG_AF(log_manager_.get())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because there is no Fast Checkout support for form "
           "signatures {"
        << form_signature.value() << ", " << field.host_form_signature.value()
        << "} on origin " << form.main_frame_origin.Serialize() << ".";
  }
  return is_trigger_form;
#else
  NOTREACHED();
  return false;
#endif
}

bool ChromeAutofillClient::ShowFastCheckout(
    base::WeakPtr<FastCheckoutDelegate> delegate) {
#if BUILDFLAG(IS_ANDROID)
  if (delegate->IsShowingFastCheckoutUI()) {
    LOG_AF(log_manager_.get())
        << LoggingScope::kFastCheckout << LogMessage::kFastCheckout
        << "not triggered because Fast Checkout UI is already showing.";
    return false;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  return FastCheckoutClient::GetOrCreateForWebContents(web_contents())
      ->Start(delegate, url);
#else
  NOTREACHED();
  return false;
#endif
}

void ChromeAutofillClient::HideFastCheckout() {
#if BUILDFLAG(IS_ANDROID)
  FastCheckoutClient::GetOrCreateForWebContents(web_contents())->Stop();
#else
  NOTREACHED();
#endif
}

bool ChromeAutofillClient::IsTouchToFillCreditCardSupported() {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(
      features::kAutofillTouchToFillForCreditCardsAndroid);
#else
  // Touch To Fill is not supported on Desktop.
  return false;
#endif
}

bool ChromeAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const autofill::CreditCard* const> cards_to_suggest) {
#if BUILDFLAG(IS_ANDROID)
  return touch_to_fill_credit_card_controller_.Show(
      std::make_unique<TouchToFillCreditCardViewImpl>(web_contents()), delegate,
      std::move(cards_to_suggest));
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
  return false;
#endif
}

void ChromeAutofillClient::HideTouchToFillCreditCard() {
#if BUILDFLAG(IS_ANDROID)
  touch_to_fill_credit_card_controller_.Hide();
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

void ChromeAutofillClient::ShowAutofillPopup(
    const autofill::AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  // Autofill popups should only be shown in focused windows because on Windows
  // the popup may overlap the focused window (see crbug.com/1239760).
  if (!has_focus_)
    return;

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
                          open_args.autoselect_first_suggestion,
                          open_args.popup_type);

  // When testing, try to keep popup open when the reason to hide is from an
  // external browser frame resize that is extraneous to our testing goals.
  if (keep_popup_open_for_testing_ && popup_controller_.get()) {
    popup_controller_->KeepPopupOpenForTesting();
  }
}

void ChromeAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
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
  popup_controller_->Show(suggestions, AutoselectFirstSuggestion(false),
                          popup_type);
}

void ChromeAutofillClient::HideAutofillPopup(PopupHidingReason reason) {
  if (popup_controller_.get())
    popup_controller_->Hide(reason);
}

void ChromeAutofillClient::UpdateOfferNotification(
    const AutofillOfferData* offer,
    bool notification_has_been_shown) {
  DCHECK(offer);
  CreditCard* card =
      offer->GetEligibleInstrumentIds().empty()
          ? nullptr
          : GetPersonalDataManager()->GetCreditCardByInstrumentId(
                offer->GetEligibleInstrumentIds()[0]);

  if (offer->IsCardLinkedOffer() && !card)
    return;

#if BUILDFLAG(IS_ANDROID)
  if (notification_has_been_shown) {
    // For Android, if notification has been shown on this merchant, don't show
    // it again.
    return;
  }
  OfferNotificationControllerAndroid::CreateForWebContents(web_contents());
  OfferNotificationControllerAndroid* controller =
      OfferNotificationControllerAndroid::FromWebContents(web_contents());
  controller->ShowIfNecessary(offer, card);
#else
  OfferNotificationBubbleControllerImpl::CreateForWebContents(web_contents());
  OfferNotificationBubbleControllerImpl* controller =
      OfferNotificationBubbleControllerImpl::FromWebContents(web_contents());
  controller->ShowOfferNotificationIfApplicable(
      offer, card, /*should_show_icon_only_=*/notification_has_been_shown);
#endif
}

void ChromeAutofillClient::DismissOfferNotification() {
#if BUILDFLAG(IS_ANDROID)
  OfferNotificationControllerAndroid::CreateForWebContents(web_contents());
  OfferNotificationControllerAndroid* controller =
      OfferNotificationControllerAndroid::FromWebContents(web_contents());
  controller->Dismiss();
#else
  OfferNotificationBubbleControllerImpl* controller =
      OfferNotificationBubbleControllerImpl::FromWebContents(web_contents());
  if (!controller)
    return;

  controller->DismissNotification();
#endif
}

void ChromeAutofillClient::OnVirtualCardDataAvailable(
    const VirtualCardManualFallbackBubbleOptions& options) {
  GetFormDataImporter()->CacheFetchedVirtualCard(
      options.virtual_card.LastFourDigits());
#if BUILDFLAG(IS_ANDROID)
  // Show the virtual card snackbar only if the ManualFillingComponent component
  // is enabled for credit cards.
  if (features::IsAutofillManualFallbackEnabled() ||
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableManualFallbackForVirtualCards)) {
    (new AutofillSnackbarControllerImpl(web_contents()))->Show();
  }
#else
  VirtualCardManualFallbackBubbleControllerImpl::CreateForWebContents(
      web_contents());
  VirtualCardManualFallbackBubbleControllerImpl* controller =
      VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
          web_contents());
  controller->ShowBubble(options);
#endif
}

void ChromeAutofillClient::ShowVirtualCardErrorDialog(
    const AutofillErrorDialogContext& context) {
  autofill_error_dialog_controller_.Show(context);
}

void ChromeAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  DCHECK(autofill_progress_dialog_controller_);
  autofill_progress_dialog_controller_->ShowDialog(
      autofill_progress_dialog_type, std::move(cancel_callback));
}

void ChromeAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing) {
  DCHECK(autofill_progress_dialog_controller_);
  autofill_progress_dialog_controller_->DismissDialog(
      show_confirmation_before_closing);
}

bool ChromeAutofillClient::IsAutocompleteEnabled() const {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

bool ChromeAutofillClient::IsPasswordManagerEnabled() {
  PasswordManagerSettingsService* settings_service =
      PasswordManagerSettingsServiceFactory::GetForProfile(GetProfile());
  return settings_service->IsSettingEnabled(
      password_manager::PasswordManagerSetting::kOfferToSavePasswords);
}

void ChromeAutofillClient::PropagateAutofillPredictions(
    AutofillDriver* autofill_driver,
    const std::vector<FormStructure*>& forms) {
  // This cast is safe because all non-iOS clients use ContentAutofillDriver as
  // AutofillDriver implementation.
  content::RenderFrameHost* rfh =
      static_cast<ContentAutofillDriver*>(autofill_driver)->render_frame_host();
  password_manager::ContentPasswordManagerDriver* password_manager_driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  if (password_manager_driver) {
    password_manager_driver->GetPasswordManager()->ProcessAutofillPredictions(
        password_manager_driver, forms);
  }
}

void ChromeAutofillClient::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {
#if BUILDFLAG(IS_ANDROID)
  AutofillLoggerAndroid::DidFillOrPreviewField(autofilled_value,
                                               profile_full_name);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromeAutofillClient::IsContextSecure() const {
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
#if !BUILDFLAG(IS_ANDROID)
  return false;
#else
  return signin::ShouldShowPromo(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
#endif
}

bool ChromeAutofillClient::AreServerCardsSupported() const {
  // When in VR, server side cards are not supported.
  return !vr::VrTabHelper::IsInVr(web_contents());
}

void ChromeAutofillClient::ExecuteCommand(int id) {
#if BUILDFLAG(IS_ANDROID)
  if (id == POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO) {
    auto* window = web_contents()->GetNativeView()->GetWindowAndroid();
    if (window) {
      SigninBridge::LaunchSigninActivity(
          window, signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
    }
  }
#endif
}

void ChromeAutofillClient::OpenPromoCodeOfferDetailsURL(const GURL& url) {
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));
}

LogManager* ChromeAutofillClient::GetLogManager() const {
  return log_manager_.get();
}

FormInteractionsFlowId
ChromeAutofillClient::GetCurrentFormInteractionsFlowId() {
  constexpr base::TimeDelta max_flow_time = base::Minutes(20);
  base::Time now = AutofillClock::Now();

  if (now - flow_id_date_ > max_flow_time || now < flow_id_date_) {
    flow_id_ = FormInteractionsFlowId();
    flow_id_date_ = now;
  }
  return flow_id_;
}

void ChromeAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  risk_util::LoadRiskData(0, web_contents(), std::move(callback));
}

void ChromeAutofillClient::PrimaryMainFrameWasResized(bool width_changed) {
#if BUILDFLAG(IS_ANDROID)
  // Ignore virtual keyboard showing and hiding a strip of suggestions.
  if (!width_changed)
    return;
#endif

  HideAutofillPopup(PopupHidingReason::kWidgetChanged);
  // Do not need to hide the Touch To Fill surface, since it is an overlay UI
  // which doesn't depend on frame size.
}

void ChromeAutofillClient::WebContentsDestroyed() {
  HideAutofillPopup(PopupHidingReason::kTabGone);
  if (IsTouchToFillCreditCardSupported())
    HideTouchToFillCreditCard();
}

void ChromeAutofillClient::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  has_focus_ = false;
  HideAutofillPopup(PopupHidingReason::kFocusChanged);
  // Should not hide the Touch To Fill surface, since it is an overlay UI
  // which takes the focus.
}

void ChromeAutofillClient::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  has_focus_ = true;
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeAutofillClient::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  HideAutofillPopup(PopupHidingReason::kContentAreaMoved);
  // Touch To Fill is not supported on Desktop.
}
#endif  // !BUILDFLAG(IS_ANDROID)

ChromeAutofillClient::ChromeAutofillClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeAutofillClient>(*web_contents),
      content::WebContentsObserver(web_contents),
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
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())),
      autofill_error_dialog_controller_(web_contents),
      autofill_progress_dialog_controller_(
          std::make_unique<AutofillProgressDialogControllerImpl>(
              web_contents)) {
  // TODO(crbug.com/928595): Replace the closure with a callback to the
  // renderer that indicates if log messages should be sent from the
  // renderer.
  log_manager_ =
      LogManager::Create(AutofillLogRouterFactory::GetForBrowserContext(
                             web_contents->GetBrowserContext()),
                         base::NullCallback());
  // Initialize StrikeDatabase so its cache will be loaded and ready to use
  // when requested by other Autofill classes.
  GetStrikeDatabase();

#if !BUILDFLAG(IS_ANDROID)
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

bool ChromeAutofillClient::IsMultipleAccountUser() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  return identity_manager->GetAccountsWithRefreshTokens().size() > 1;
}

std::u16string ChromeAutofillClient::GetAccountHolderName() {
  Profile* profile = GetProfile();
  if (!profile)
    return std::u16string();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return std::u16string();
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync));
  return base::UTF8ToUTF16(primary_account_info.full_name);
}

std::u16string ChromeAutofillClient::GetAccountHolderEmail() {
  Profile* profile = GetProfile();
  if (!profile)
    return std::u16string();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)
    return std::u16string();
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync));
  return base::UTF8ToUTF16(primary_account_info.email);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeAutofillClient);

}  // namespace autofill
