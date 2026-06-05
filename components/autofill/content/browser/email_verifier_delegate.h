// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_EMAIL_VERIFIER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_EMAIL_VERIFIER_DELEGATE_H_

#include <map>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webid/email_verifier.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(EvpAutofillFlowResult)
enum class EvpAutofillFlowResult {
  kSuccess = 0,
  kTokenFieldHasNoNonce = 1,
  kUserPrefDisabled = 2,
  kStrikeDatabaseBlock = 3,
  kVerifierUnavailable = 4,
  kNotVerifiable = 5,
  kUserDeclinedPermissionPrompt = 6,
  kUserIgnoredPermissionPrompt = 7,
  kVerificationFailed = 8,
  kManagerDestroyed = 9,
  kTokenSentToRenderer = 10,
  kMaxValue = kTokenSentToRenderer,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:EvpAutofillFlowResult)

// The EmailVerifierDelegate is owned by the ChromeAutofillClient (and hence is
// one per WebContents) and observes all AutofillManagers.
// It listens to filling events and triggers the email verification flow if an
// participating input field (one that has a "nonce" attribute) is filled with
// an email address.
//
// https://github.com/dickhardt/email-verification-protocol
class EmailVerifierDelegate : public AutofillManager::Observer,
                              public content::WebContentsObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnFlowCompleted(EvpAutofillFlowResult result) = 0;
  };

  explicit EmailVerifierDelegate(AutofillClient* client);
  EmailVerifierDelegate(const EmailVerifierDelegate&) = delete;
  EmailVerifierDelegate& operator=(const EmailVerifierDelegate&) = delete;

  ~EmailVerifierDelegate() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // AutofillManager::Observer:
  void OnFillOrPreviewForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId trigger_field_id,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const FillingPayload& filling_payload) override;
  void OnFillOrPreviewField(AutofillManager& manager,
                            FormGlobalId form_id,
                            FieldGlobalId field_id,
                            mojom::ActionPersistence action_persistence,
                            const std::u16string& value,
                            std::optional<FieldType> field_type_used) override;
  void OnEmailVerificationTokenShared(AutofillManager& manager,
                                      FieldGlobalId field_id) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Initiates the verification of the given `email_value` by checking the frame
  // for a `nonce` attribute, prompting the user for verification, and sending
  // the token to the renderer on completion.
  void TriggerVerification(AutofillManager& manager,
                           const FormStructure& form,
                           const AutofillField& email_field,
                           const std::u16string& email_value);

  void OnIsVerifiable(
      base::WeakPtr<AutofillManager> manager,
      FieldGlobalId email_field_id,
      FieldGlobalId token_field_id,
      gfx::RectF email_field_bounds,
      std::u16string email,
      std::string nonce,
      bool already_allowed,
      std::optional<content::webid::EmailVerifier::Result> result);

  void Verify(base::WeakPtr<AutofillManager> manager,
              FieldGlobalId email_field_id,
              std::string email_utf8,
              FieldGlobalId token_field_id,
              const std::string& nonce,
              const content::webid::EmailVerifier::Result& result);

  void OnVerificationResponseReceived(base::WeakPtr<AutofillManager> manager,
                                      FieldGlobalId email_field_id,
                                      std::string email,
                                      FieldGlobalId token_field_id,
                                      net::SchemefulSite issuer_site,
                                      std::optional<std::string> token);

  void OnEmailVerificationDecision(
      base::WeakPtr<AutofillManager> manager,
      FieldGlobalId email_field_id,
      std::string email_utf8,
      FieldGlobalId token_field_id,
      std::string nonce,
      content::webid::EmailVerifier::Result result,
      AutofillClient::EmailVerificationPermissionUiResult ui_result);

  void NotifyFlowCompleted(EvpAutofillFlowResult result);

  class MetricsObserver : public Observer {
   public:
    MetricsObserver();
    ~MetricsObserver() override;
    void OnFlowCompleted(EvpAutofillFlowResult result) override;
  };

  MetricsObserver metrics_observer_;
  base::ObserverList<Observer> observers_;

  ScopedAutofillManagersObservation observation_{this};
  std::map<FieldGlobalId, GURL> issuers_;
  base::WeakPtrFactory<EmailVerifierDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_EMAIL_VERIFIER_DELEGATE_H_
