// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "chrome/browser/autofill/autofill_gstatic_reader.h"
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
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/preferences/preferences_launcher.h"
#include "chrome/browser/android/signin/signin_promo_util_android.h"
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
#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/verify_pending_dialog_view.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/zoom/zoom_controller.h"
#endif

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
  if (base::FeatureList::IsEnabled(features::kAutofillAddressNormalizer))
    return AddressNormalizerFactory::GetInstance();
  return nullptr;
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

std::string ChromeAutofillClient::GetPageLanguage() const {
  // TODO(crbug.com/912597): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents());
  if (translate_manager)
    return translate_manager->GetLanguageState().original_language();
  return std::string();
}

void ChromeAutofillClient::ShowAutofillSettings(
    bool show_credit_card_settings) {
#if defined(OS_ANDROID)
  if (show_credit_card_settings) {
    chrome::android::PreferencesLauncher::ShowAutofillCreditCardSettings(
        web_contents());
  } else {
    chrome::android::PreferencesLauncher::ShowAutofillProfileSettings(
        web_contents());
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
      base::Bind(&CreateCardUnmaskPromptView,
                 base::Unretained(&unmask_controller_),
                 base::Unretained(web_contents())),
      card, reason, delegate);
}

void ChromeAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void ChromeAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
#if !defined(OS_ANDROID)
  autofill::ManageMigrationUiController::CreateForWebContents(web_contents());
  autofill::ManageMigrationUiController* controller =
      autofill::ManageMigrationUiController::FromWebContents(web_contents());
  controller->ShowBubble(std::move(show_migration_dialog_closure));
#endif
}

void ChromeAutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
#if !defined(OS_ANDROID)
  autofill::ManageMigrationUiController::CreateForWebContents(web_contents());
  autofill::ManageMigrationUiController* controller =
      autofill::ManageMigrationUiController::FromWebContents(web_contents());
  controller->ShowOfferDialog(legal_message_lines, user_email,
                              migratable_credit_cards,
                              std::move(start_migrating_cards_callback));
#endif
}

void ChromeAutofillClient::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const base::string16& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {
#if !defined(OS_ANDROID)
  autofill::ManageMigrationUiController::CreateForWebContents(web_contents());
  autofill::ManageMigrationUiController* controller =
      autofill::ManageMigrationUiController::FromWebContents(web_contents());
  controller->UpdateCreditCardIcon(has_server_error, tip_message,
                                   migratable_credit_cards,
                                   delete_local_card_callback);
#endif
}

#if !defined(OS_ANDROID)
void ChromeAutofillClient::ShowVerifyPendingDialog(
    base::OnceClosure cancel_card_verification_callback) {
  autofill::VerifyPendingDialogControllerImpl::CreateForWebContents(
      web_contents());
  autofill::VerifyPendingDialogControllerImpl::FromWebContents(web_contents())
      ->ShowDialog(std::move(cancel_card_verification_callback));
}

void ChromeAutofillClient::CloseVerifyPendingDialog() {
  VerifyPendingDialogControllerImpl* controller =
      autofill::VerifyPendingDialogControllerImpl::FromWebContents(
          web_contents());
  if (!controller)
    return;

  controller->OnCardVerificationCompleted();
}
#endif

void ChromeAutofillClient::ShowWebauthnOfferDialog(
    WebauthnOfferDialogCallback callback) {
#if !defined(OS_ANDROID)
  autofill::WebauthnOfferDialogControllerImpl::CreateForWebContents(
      web_contents());
  autofill::WebauthnOfferDialogControllerImpl::FromWebContents(web_contents())
      ->ShowOfferDialog(std::move(callback));
#endif
}

bool ChromeAutofillClient::CloseWebauthnOfferDialog() {
#if !defined(OS_ANDROID)
  WebauthnOfferDialogControllerImpl* controller =
      autofill::WebauthnOfferDialogControllerImpl::FromWebContents(
          web_contents());
  if (controller)
    return controller->CloseDialog();
#endif
  return false;
}

void ChromeAutofillClient::UpdateWebauthnOfferDialogWithError() {
#if !defined(OS_ANDROID)
  WebauthnOfferDialogControllerImpl* controller =
      autofill::WebauthnOfferDialogControllerImpl::FromWebContents(
          web_contents());
  if (controller)
    controller->UpdateDialogWithError();
#endif
}

void ChromeAutofillClient::ConfirmSaveAutofillProfile(
    const AutofillProfile& profile,
    base::OnceClosure callback) {
  // Since there is no confirmation needed to save an Autofill Profile,
  // running |callback| will proceed with saving |profile|.
  std::move(callback).Run();
}

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
              /*local_save_card_callback=*/std::move(callback), GetPrefs(),
              payments_client_->is_off_the_record())));
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(
      web_contents());
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferLocalSave(card, options, std::move(callback));
#endif
}

#if defined(OS_ANDROID)
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
              AutofillClient::LocalSaveCardPromptCallback(), GetPrefs(),
              payments_client_->is_off_the_record());
  InfoBarService::FromWebContents(web_contents())
      ->AddInfoBar(CreateSaveCardInfoBarMobile(
          std::move(save_card_info_bar_delegate_mobile)));
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferUploadSave(card, legal_message_lines, options,
                              std::move(callback));
#endif
}

void ChromeAutofillClient::CreditCardUploadCompleted(bool card_saved) {
#if defined(OS_ANDROID)
  // TODO(hozhng@): Placeholder for Clank Notification.
#else
  if (!base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback)) {
    return;
  }

  // Do lazy initialization of SaveCardBubbleControllerImpl.
  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents());
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

void ChromeAutofillClient::ScanCreditCard(
    const CreditCardScanCallback& callback) {
  CreditCardScannerController::ScanCreditCard(web_contents(), callback);
}

void ChromeAutofillClient::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<autofill::Suggestion>& suggestions,
    bool autoselect_first_suggestion,
    PopupType popup_type,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();

  // Will delete or reuse the old |popup_controller_|.
  popup_controller_ =
      AutofillPopupControllerImpl::GetOrCreate(popup_controller_,
                                               delegate,
                                               web_contents(),
                                               web_contents()->GetNativeView(),
                                               element_bounds_in_screen_space,
                                               text_direction);

  popup_controller_->Show(suggestions, autoselect_first_suggestion, popup_type);
}

void ChromeAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  if (popup_controller_.get())
    popup_controller_->UpdateDataListValues(values, labels);
}

void ChromeAutofillClient::HideAutofillPopup() {
  if (popup_controller_.get())
    popup_controller_->Hide();
}

bool ChromeAutofillClient::IsAutocompleteEnabled() {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

void ChromeAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {
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
  // Note: Defer to SecurityStateTabHelper to determine what pages
  // are secure so that autofill behavior matches that shown in the omnibox.

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());

  // There may be no SecurityStateTabHelper attached in some tests.
  return helper &&
         security_state::IsSslCertificateValid(helper->GetSecurityLevel());
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
  if (id == autofill::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO) {
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
  ::autofill::LoadRiskData(0, web_contents(), std::move(callback));
}

void ChromeAutofillClient::MainFrameWasResized(bool width_changed) {
#if defined(OS_ANDROID)
  // Ignore virtual keyboard showing and hiding a strip of suggestions.
  if (!width_changed)
    return;
#endif

  HideAutofillPopup();
}

void ChromeAutofillClient::WebContentsDestroyed() {
  HideAutofillPopup();
}

void ChromeAutofillClient::DidAttachInterstitialPage() {
  HideAutofillPopup();
}

#if !defined(OS_ANDROID)
void ChromeAutofillClient::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  HideAutofillPopup();
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
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext()),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->IsOffTheRecord()) {
  if (::autofill::prefs::IsCreditCardAutofillEnabled(GetPrefs()))
    AutofillGstaticReader::GetInstance()->SetUp();
  // TODO(crbug.com/928595): Replace the closure with a callback to the renderer
  // that indicates if log messages should be sent from the renderer.
  log_manager_ =
      LogManager::Create(AutofillLogRouterFactory::GetForBrowserContext(
                             web_contents->GetBrowserContext()),
                         base::Closure());

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
