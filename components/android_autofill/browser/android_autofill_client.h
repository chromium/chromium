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
#include "components/autofill/core/browser/autofill_trigger_details.h"
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
class AndroidAutofillClient : public autofill::ContentAutofillClient {
 public:
  static void CreateForWebContents(content::WebContents* contents);

  AndroidAutofillClient(const AndroidAutofillClient&) = delete;
  AndroidAutofillClient& operator=(const AndroidAutofillClient&) = delete;

  ~AndroidAutofillClient() override;

  // AutofillClient:
  bool IsOffTheRecord() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  autofill::AutofillCrowdsourcingManager* GetCrowdsourcingManager() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager()
      override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  const signin::IdentityManager* GetIdentityManager() const override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  void ShowAutofillSettings(autofill::SuggestionType suggestion_type) override;
  void ConfirmSaveAddressProfile(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      bool is_migration_to_account,
      AddressProfileSavePromptCallback callback) override;
  void ShowEditAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
  SuggestionUiSessionId ShowAutofillSuggestions(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillSuggestionDelegate> delegate) override;
  void UpdateAutofillDataListValues(
      base::span<const autofill::SelectOption> datalist) override;
  void PinAutofillSuggestions() override;
  void HideAutofillSuggestions(
      autofill::SuggestionHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(
      autofill::mojom::ActionPersistence action_persistence,
      autofill::AutofillTriggerSource trigger_source,
      bool is_refill) override;
  bool IsContextSecure() const override;
  autofill::FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;

  // ContentAutofillClient:
  std::unique_ptr<autofill::AutofillManager> CreateManager(
      base::PassKey<autofill::ContentAutofillDriver> pass_key,
      autofill::ContentAutofillDriver& driver) override;

 protected:
  // Protected for testing.
  explicit AndroidAutofillClient(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<AndroidAutofillClient>;

  content::WebContents& GetWebContents() const;

  JavaObjectWeakGlobalRef java_ref_;

  std::unique_ptr<autofill::AutofillCrowdsourcingManager>
      crowdsourcing_manager_;
};

}  // namespace android_autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_CLIENT_H_
