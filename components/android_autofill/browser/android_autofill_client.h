// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_CLIENT_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/android/view_android.h"

namespace autofill {
class AutocompleteHistoryManager;
class AutofillSuggestionDelegate;
class PersonalDataManager;
class StrikeDatabase;
enum class SuggestionType;
}  // namespace autofill

namespace content {
class WebContents;
}

namespace syncer {
class SyncService;
}

class PersonalDataManager;
class PrefService;

namespace android_autofill {

// One Android implementation of the AutofillClient. If used, the Android
// Autofill framework is responsible for autofill and password management.
//
// This client is
//   a) always used on WebView, and
//   b) used on Clank if users switch to a 3P provider and thus disable the
//      built-in `BrowserAutofillManager`.
//
// By using this client, the embedder responds to requests from the Android
// Autofill API asking for a "virtual view structure". This class defers to
// parsing logic in renderer and components/autofill to identify forms. The
// forms are translated into that virtual view structure by the Java-side of
// this component (see AndroidAutofillClient.java using AutofillProvider.java).
// Any higher layer only needs to forward the API requests to this client. The
// same applies to filling: the data filling happens in renderer code and only
// requires the embedder to forward the data to be filled.
// The UI (except for datalist dropdowns) are handled entirely by the Platform.
// Neither WebView nor Chrome can control whether e.g. a dropdown or
// keyboard-inlined suggestion are served to the user.
//
// It is created by either AwContents or ChromeAutofillClient and owned by the
// WebContents that it is attached to.
//
// BEWARE OF SUBCLASSING in tests: virtual function calls during construction
// may lead to very surprising behavior. The class is not `final` because one
// test derives from it. Member functions should be final unless they need to be
// mocked or overridden in subclasses and you have verified that they are not
// called, directly or indirectly, from the constructor.
class AndroidAutofillClient : public autofill::ContentAutofillClient {
 public:
  static void CreateForWebContents(content::WebContents* contents);

  AndroidAutofillClient(const AndroidAutofillClient&) = delete;
  AndroidAutofillClient& operator=(const AndroidAutofillClient&) = delete;

  ~AndroidAutofillClient() override;

  // AutofillClient:
  base::WeakPtr<AutofillClient> GetWeakPtr() final;
  const std::string& GetAppLocale() const final;
  bool IsOffTheRecord() const final;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() final;
  autofill::AutofillCrowdsourcingManager& GetCrowdsourcingManager() final;
  autofill::VotesUploader& GetVotesUploader() override;
  autofill::PersonalDataManager& GetPersonalDataManager() final;
  autofill::ValuablesDataManager* GetValuablesDataManager() override;
  autofill::EntityDataManager* GetEntityDataManager() override;
  autofill::SingleFieldFillRouter& GetSingleFieldFillRouter() final;
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager() final;
  PrefService* GetPrefs() final;
  const PrefService* GetPrefs() const final;
  syncer::SyncService* GetSyncService() final;
  signin::IdentityManager* GetIdentityManager() final;
  const signin::IdentityManager* GetIdentityManager() const final;
  autofill::FormDataImporter* GetFormDataImporter() final;
  autofill::StrikeDatabase* GetStrikeDatabase() final;
  ukm::UkmRecorder* GetUkmRecorder() final;
  autofill::AddressNormalizer* GetAddressNormalizer() final;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const final;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const final;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() final;
  const translate::LanguageState* GetLanguageState() final;
  translate::TranslateDriver* GetTranslateDriver() final;
  void ShowAutofillSettings(autofill::SuggestionType suggestion_type) final;
  void ConfirmSaveAddressProfile(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      bool is_migration_to_account,
      AddressProfileSavePromptCallback callback) final;
  SuggestionUiSessionId ShowAutofillSuggestions(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillSuggestionDelegate> delegate) final;
  void UpdateAutofillDataListValues(
      base::span<const autofill::SelectOption> datalist) final;
  void HideAutofillSuggestions(autofill::SuggestionHidingReason reason) final;
  bool IsAutofillEnabled() const final;
  bool IsAutofillProfileEnabled() const final;
  bool IsAutofillPaymentMethodsEnabled() const final;
  bool IsAutocompleteEnabled() const final;
  bool IsPasswordManagerEnabled() const final;
  void DidFillForm(autofill::AutofillTriggerSource trigger_source,
                   bool is_refill) final;
  bool IsContextSecure() const final;
  autofill::FormInteractionsFlowId GetCurrentFormInteractionsFlowId() final;
  autofill::autofill_metrics::FormInteractionsUkmLogger&
  GetFormInteractionsUkmLogger() final;

  // ContentAutofillClient:
  std::unique_ptr<autofill::AutofillManager> CreateManager(
      base::PassKey<autofill::ContentAutofillDriver> pass_key,
      autofill::ContentAutofillDriver& driver) final;

 protected:
  // Protected for testing.
  explicit AndroidAutofillClient(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<AndroidAutofillClient>;

  content::WebContents& GetWebContents() const;

  JavaObjectWeakGlobalRef java_ref_;

  std::unique_ptr<autofill::AutofillCrowdsourcingManager>
      crowdsourcing_manager_;

  autofill::VotesUploader votes_uploader_{this};

  autofill::autofill_metrics::FormInteractionsUkmLogger
      form_interactions_ukm_logger_{this};

  base::WeakPtrFactory<AndroidAutofillClient> weak_ptr_factory_{this};
};

}  // namespace android_autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_CLIENT_H_
