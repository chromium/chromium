// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/credit_card_scanner_controller.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_view.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_metrics.h"
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
#include "chrome/browser/ui/android/infobars/autofill_credit_card_filling_infobar.h"
#include "components/autofill/core/browser/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_mobile.h"
#include "components/infobars/core/infobar.h"
#include "ui/android/window_android.h"
#else  // !OS_ANDROID
#include "chrome/browser/ui/autofill/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/zoom/zoom_controller.h"
#endif

namespace autofill {

ChromeAutofillClient::ChromeAutofillClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      unmask_controller_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext()),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->IsOffTheRecord()) {
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

ChromeAutofillClient::~ChromeAutofillClient() {
  // NOTE: It is too late to clean up the autofill popup; that cleanup process
  // requires that the WebContents instance still be valid and it is not at
  // this point (in particular, the WebContentsImpl destructor has already
  // finished running and we are now in the base class destructor).
  DCHECK(!popup_controller_);
}

PersonalDataManager* ChromeAutofillClient::GetPersonalDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return PersonalDataManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

scoped_refptr<AutofillWebDataService> ChromeAutofillClient::GetDatabase() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}

PrefService* ChromeAutofillClient::GetPrefs() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext())
      ->GetPrefs();
}

syncer::SyncService* ChromeAutofillClient::GetSyncService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
}

identity::IdentityManager* ChromeAutofillClient::GetIdentityManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
}

StrikeDatabase* ChromeAutofillClient::GetStrikeDatabase() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  // No need to return a StrikeDatabase in incognito mode. It is primarily used
  // to determine whether or not to offer save of Autofill data. However, we
  // don't allow saving of Autofill data while in incognito anyway, so an
  // incognito code path should never get far enough to query StrikeDatabase.
  DCHECK(!profile->IsOffTheRecord());
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

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  return security_info.security_level;
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
      chrome::ShowSettingsSubPage(browser, chrome::kAutofillSubPage);
    }
  }
#endif  // #if defined(OS_ANDROID)
}

void ChromeAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      CreateCardUnmaskPromptView(&unmask_controller_, web_contents()),
      card, reason, delegate);
}

void ChromeAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void ChromeAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
#if !defined(OS_ANDROID)
  autofill::LocalCardMigrationBubbleControllerImpl::CreateForWebContents(
      web_contents());
  autofill::LocalCardMigrationBubbleControllerImpl* controller =
      autofill::LocalCardMigrationBubbleControllerImpl::FromWebContents(
          web_contents());
  controller->ShowBubble(std::move(show_migration_dialog_closure));
#endif
}

void ChromeAutofillClient::ConfirmMigrateLocalCardToCloud(
    std::unique_ptr<base::DictionaryValue> legal_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
#if !defined(OS_ANDROID)
  autofill::LocalCardMigrationDialogControllerImpl::CreateForWebContents(
      web_contents());
  autofill::LocalCardMigrationDialogControllerImpl* controller =
      autofill::LocalCardMigrationDialogControllerImpl::FromWebContents(
          web_contents());
  controller->ShowDialog(
      std::move(legal_message),
      CreateLocalCardMigrationDialogView(controller, web_contents()),
      migratable_credit_cards, std::move(start_migrating_cards_callback));
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
    bool show_prompt,
    base::OnceClosure callback) {
#if defined(OS_ANDROID)
  DCHECK(show_prompt);
  InfoBarService::FromWebContents(web_contents())
      ->AddInfoBar(CreateSaveCardInfoBarMobile(
          std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
              false, card, std::unique_ptr<base::DictionaryValue>(nullptr),
              GetStrikeDatabase(),
              /*upload_save_card_callback=*/
              base::OnceCallback<void(const base::string16&)>(),
              /*local_save_card_callback=*/std::move(callback), GetPrefs())));
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(
      web_contents());
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferLocalSave(card, show_prompt, std::move(callback));
#endif
}

void ChromeAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_request_name_from_user,
    bool show_prompt,
    base::OnceCallback<void(const base::string16&)> callback) {
#if defined(OS_ANDROID)
  DCHECK(show_prompt);
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
      save_card_info_bar_delegate_mobile =
          std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
              true, card, std::move(legal_message), GetStrikeDatabase(),
              /*upload_save_card_callback=*/std::move(callback),
              /*local_save_card_callback=*/base::Closure(), GetPrefs());
  if (save_card_info_bar_delegate_mobile->LegalMessagesParsedSuccessfully()) {
    InfoBarService::FromWebContents(web_contents())
        ->AddInfoBar(CreateSaveCardInfoBarMobile(
            std::move(save_card_info_bar_delegate_mobile)));
  }
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->OfferUploadSave(card, std::move(legal_message),
                              should_request_name_from_user, show_prompt,
                              std::move(callback));
#endif
}

void ChromeAutofillClient::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    const base::Closure& callback) {
#if defined(OS_ANDROID)
  auto infobar_delegate =
      std::make_unique<AutofillCreditCardFillingInfoBarDelegateMobile>(
          card, callback);
  auto* raw_delegate = infobar_delegate.get();
  if (InfoBarService::FromWebContents(web_contents())
          ->AddInfoBar(std::make_unique<AutofillCreditCardFillingInfoBar>(
              std::move(infobar_delegate)))) {
    raw_delegate->set_was_shown();
  }
#endif
}

void ChromeAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  ::autofill::LoadRiskData(0, web_contents(), std::move(callback));
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

  popup_controller_->Show(suggestions, autoselect_first_suggestion);
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

  // Password generation popups behave in the same fashion and should also
  // be hidden.
  ChromePasswordManagerClient* password_client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
  if (password_client)
    password_client->HidePasswordGenerationPopup();
}

bool ChromeAutofillClient::IsAutocompleteEnabled() {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

bool ChromeAutofillClient::AreServerCardsSupported() {
  // When in VR, server side cards are not supported.
  return !vr::VrTabHelper::IsInVr(web_contents());
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

void ChromeAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  if (driver) {
    driver->GetPasswordGenerationManager()->ProcessPasswordRequirements(forms);
    driver->GetPasswordGenerationManager()->DetectFormsEligibleForGeneration(
        forms);
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

void ChromeAutofillClient::DidInteractWithNonsecureCreditCardInput() {
  InsecureSensitiveInputDriverFactory* factory =
      InsecureSensitiveInputDriverFactory::GetOrCreateForWebContents(
          web_contents());
  factory->DidInteractWithNonsecureCreditCardInput();
}

bool ChromeAutofillClient::IsContextSecure() {
  content::SSLStatus ssl_status;
  content::NavigationEntry* navigation_entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!navigation_entry)
     return false;

  ssl_status = navigation_entry->GetSSL();
  // Note: If changing the implementation below, also change
  // AwAutofillClient::IsContextSecure. See crbug.com/505388
  return navigation_entry->GetURL().SchemeIsCryptographic() &&
         ssl_status.certificate &&
         (!net::IsCertStatusError(ssl_status.cert_status) ||
          net::IsCertStatusMinorError(ssl_status.cert_status)) &&
         !(ssl_status.content_status &
           content::SSLStatus::RAN_INSECURE_CONTENT);
}

bool ChromeAutofillClient::ShouldShowSigninPromo() {
#if !defined(OS_ANDROID)
  return false;
#else
  return signin::ShouldShowPromo(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
#endif
}

void ChromeAutofillClient::ExecuteCommand(int id) {
  if (id == autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY) {
#if !defined(OS_ANDROID)
    chrome::ShowSettingsSubPage(
        chrome::FindBrowserWithWebContents(web_contents()),
        chrome::kPasswordManagerSubPage);
#else
    chrome::android::PreferencesLauncher::ShowPasswordSettings();
#endif
  } else if (id == autofill::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO) {
#if defined(OS_ANDROID)
    auto* window = web_contents()->GetNativeView()->GetWindowAndroid();
    if (window) {
      chrome::android::SigninPromoUtilAndroid::
          StartAccountSigninActivityForPromo(
              window,
              signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
    }
#endif
  }
}

}  // namespace autofill
