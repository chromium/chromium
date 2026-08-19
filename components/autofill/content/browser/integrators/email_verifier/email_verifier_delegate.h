// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_EMAIL_VERIFIER_EMAIL_VERIFIER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_EMAIL_VERIFIER_EMAIL_VERIFIER_DELEGATE_H_

#include <map>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
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
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/webid/email_verification_request.mojom-shared.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(EvpAutofillFlowResult)
enum class EvpAutofillFlowResult {
  kSuccess = 0,  // Obsolete.
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
  kDriverInactive = 11,
  kPageNavigatedDuringVerification = 12,
  kPageNavigatedDuringCheckIfVerifiable = 13,
  kMaxValue = kPageNavigatedDuringCheckIfVerifiable,
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
  // Aggregates status and timing metrics recorded throughout a single Email
  // Verification Protocol (EVP) request attempt. This struct buffers stage
  // metrics so that a single aggregated UKM event
  // (`Blink.EmailVerificationProtocol`) can be emitted when the flow completes.
  struct RequestMetrics {
    // UKM source ID associated with the main frame page navigation.
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;

    // High-level outcome of the EVP flow from the Autofill perspective.
    std::optional<EvpAutofillFlowResult> autofill_flow_result;

    // Outcome and dismissal status of the permission UI prompt.
    std::optional<AutofillClient::EmailVerificationPermissionUiStatus>
        permission_ui_status;

    // Status outcome of the IsVerifiable check.
    std::optional<blink::mojom::EmailVerificationRequestResult>
        is_verifiable_status;

    // Duration of the IsVerifiable check.
    std::optional<base::TimeDelta> is_verifiable_duration;

    // Status outcome of the Verify stage.
    std::optional<blink::mojom::EmailVerificationRequestResult> verify_status;

    // Duration of the Verify stage.
    std::optional<base::TimeDelta> verify_duration;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnFlowCompleted(const RequestMetrics& metrics) {}
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
      const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&,
      const FillingPayload& filling_payload) override;
  void OnFillOrPreviewField(AutofillManager& manager,
                            FormGlobalId form_id,
                            FieldGlobalId field_id,
                            mojom::ActionPersistence action_persistence,
                            const std::u16string& value,
                            std::optional<FieldType> field_type_used) override;
  void OnBeforeFormWithEmailVerificationTokenSubmitted(
      AutofillManager& manager,
      const FormData& form,
      const FieldGlobalId& email_field_id) override;
  void OnAfterFocusOnFormField(AutofillManager& manager,
                               FormGlobalId form_id,
                               FieldGlobalId field_id) override;
  void OnAfterFocusOnNonFormField(AutofillManager& manager) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  class MetricsObserver : public Observer {
   public:
    MetricsObserver();
    ~MetricsObserver() override;
    void OnFlowCompleted(const RequestMetrics& metrics) override;
  };

  // Queries the renderer for the nonce.
  void QueryNonce(AutofillManager& manager,
                  const AutofillField& email_field,
                  const std::u16string& email_value);

  void OnNonceReceived(base::WeakPtr<AutofillManager> manager,
                       FieldGlobalId email_field_id,
                       gfx::RectF email_field_bounds,
                       std::u16string email_value,
                       const std::optional<std::string>& nonce);

  // Initiates the verification of the given `email_value` by checking the
  // user pref, origin trial, and strike database, prompting the user for
  // verification, and sending the token to the renderer on completion.
  void TriggerVerification(AutofillManager& manager,
                           FieldGlobalId email_field_id,
                           gfx::RectF email_field_bounds,
                           std::u16string email_value,
                           const std::string& nonce);

  void OnDnsCheckPassed(base::WeakPtr<AutofillManager> manager,
                        FieldGlobalId email_field_id);

  void OnIsVerifiable(
      base::WeakPtr<AutofillManager> manager,
      FieldGlobalId email_field_id,
      gfx::RectF email_field_bounds,
      std::u16string email,
      std::string nonce,
      bool already_allowed,
      std::optional<content::webid::EmailVerifier::Result> result,
      blink::mojom::EmailVerificationRequestResult status,
      base::TimeDelta is_verifiable_duration);

  void Verify(base::WeakPtr<AutofillManager> manager,
              FieldGlobalId email_field_id,
              std::string email_utf8,
              const std::string& nonce,
              const content::webid::EmailVerifier::Result& result);

  void OnVerificationResponseReceived(
      base::WeakPtr<AutofillManager> manager,
      FieldGlobalId email_field_id,
      std::string email,
      net::SchemefulSite issuer_site,
      std::optional<std::string> token,
      blink::mojom::EmailVerificationRequestResult status,
      base::TimeDelta verify_duration);

  void OnEmailVerificationDecision(
      base::WeakPtr<AutofillManager> manager,
      FieldGlobalId email_field_id,
      std::string email_utf8,
      std::string nonce,
      content::webid::EmailVerifier::Result result,
      AutofillClient::EmailVerificationPermissionUiStatus ui_status);

  // Notifies `observers_` that an EVP flow finished with `result`. If `manager`
  // is present and the flow ended in a state other than success or waiting for
  // renderer response, resets the email verification loading spinner on the
  // input field (`EmailVerificationState::kNone`).
  void NotifyFlowCompleted(AutofillManager* manager,
                           FieldGlobalId field_id,
                           EvpAutofillFlowResult result);

  void OnFieldLostFocus(AutofillManager& manager,
                        const FieldGlobalId& field_id);

  MetricsObserver metrics_observer_;
  base::ObserverList<Observer> observers_;

  ScopedAutofillManagersObservation observation_{this};
  std::map<FieldGlobalId, GURL> issuers_;
  // Holds the active RequestMetrics for in-flight verification requests, keyed
  // by the email field's ID. Entries are inserted in `TriggerVerification` and
  // erased in `NotifyFlowCompleted`.
  base::flat_map<FieldGlobalId, RequestMetrics> pending_request_metrics_;
  std::optional<FieldGlobalId> last_focused_field_;
  // A tab-scoped cache of recently verified email values (mapped by field ID)
  // used to deduplicate verification prompts when the user alternates focus.
  std::vector<std::pair<FieldGlobalId, std::string>> last_verified_values_;

  base::WeakPtrFactory<EmailVerifierDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_EMAIL_VERIFIER_EMAIL_VERIFIER_DELEGATE_H_
