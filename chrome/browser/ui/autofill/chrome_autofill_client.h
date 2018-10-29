// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if !defined(OS_ANDROID)
#include "components/autofill/core/browser/ui/local_card_migration_bubble_controller.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/zoom/zoom_observer.h"
#endif  // !defined(OS_ANDROID)

namespace content {
class WebContents;
}

namespace autofill {

class AutofillPopupControllerImpl;

// Chrome implementation of AutofillClient.
class ChromeAutofillClient
    : public AutofillClient,
      public content::WebContentsUserData<ChromeAutofillClient>,
      public content::WebContentsObserver
#if !defined(OS_ANDROID)
      ,
      public zoom::ZoomObserver
#endif  // !defined(OS_ANDROID)
{
 public:
  ~ChromeAutofillClient() override;

  // AutofillClient:
  PersonalDataManager* GetPersonalDataManager() override;
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  identity::IdentityManager* GetIdentityManager() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      std::unique_ptr<base::DictionaryValue> legal_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ConfirmSaveAutofillProfile(const AutofillProfile& profile,
                                  base::OnceClosure callback) override;
  void ConfirmSaveCreditCardLocally(const CreditCard& card,
                                    bool show_prompt,
                                    base::OnceClosure callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      bool should_request_name_from_user,
      bool show_prompt,
      base::OnceCallback<void(const base::string16&)> callback) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   const base::Closure& callback) override;
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
  void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<autofill::Suggestion>& suggestions,
      bool autoselect_first_suggestion,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  void HideAutofillPopup() override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  void DidInteractWithNonsecureCreditCardInput() override;
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() override;
  void ExecuteCommand(int id) override;

  // content::WebContentsObserver implementation.
  void MainFrameWasResized(bool width_changed) override;
  void WebContentsDestroyed() override;
  // Hide autofill popup if an interstitial is shown.
  void DidAttachInterstitialPage() override;

  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_for_testing() {
    return popup_controller_;
  }

#if !defined(OS_ANDROID)
  // ZoomObserver implementation.
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif  // !defined(OS_ANDROID)

 private:
  friend class content::WebContentsUserData<ChromeAutofillClient>;

  explicit ChromeAutofillClient(content::WebContents* web_contents);

  void ShowHttpNotSecureExplanation();

  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;
  CardUnmaskPromptControllerImpl unmask_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAutofillClient);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
