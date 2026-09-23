// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/email_verifier/email_verifier_delegate.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/strike_databases/evp/email_verification_not_signed_in_strike_database.h"
#include "components/autofill/core/browser/strike_databases/evp/email_verification_strike_database.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strike_database/history_clearable_strike_database.h"
#include "components/strike_database/strike_database.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

namespace {

std::optional<url::Origin> GetOriginFromEmail(std::string_view email) {
  auto parts = base::RSplitStringOnce(email, "@");
  if (!parts || parts->first.empty() || parts->second.empty()) {
    return std::nullopt;
  }
  GURL url("https://" + std::string(parts->second));
  if (!url.is_valid() || !url.has_host() || url.GetHost() != parts->second) {
    return std::nullopt;
  }
  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque()) {
    return std::nullopt;
  }
  return origin;
}

content::webid::EmailVerifier* GetOrCreateEmailVerifier(
    AutofillClient& client,
    content::RenderFrameHost* rfh,
    const std::optional<url::Origin>& email_origin = std::nullopt) {
  if (!rfh) {
    return nullptr;
  }

  std::optional<bool> overridden_state =
      base::FeatureList::GetStateIfOverridden(
          ::features::kEmailVerificationProtocol);
  if (overridden_state == std::make_optional(false)) {
    // If the flag is overridden to be disabled (e.g. via Finch), respect that.
    return nullptr;
  }

  if (overridden_state == std::make_optional(true)) {
    // If the flag is overridden to enabled, we enable no matter what the
    // OT status is.
    return content::webid::EmailVerifier::GetOrCreateForFrame(rfh);
  }

  // In the non-overridden experiment state, EVT is enabled if the feature
  // is enabled globally (via the default state) and the web site opts in
  // via Origin trial token (first-party or third-party for the email origin).
  const bool globally_enabled =
      base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol);
  content::RuntimeFeatureStateDocumentData* const document_data =
      content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(rfh);
  bool enabled_for_page =
      document_data && document_data->runtime_feature_state_read_context()
                           .IsEmailVerificationProtocolEnabled();

  if (!enabled_for_page && document_data && email_origin) {
    enabled_for_page = document_data->runtime_feature_state_read_context()
                           .IsEmailVerificationProtocolEnabledForThirdParty(
                               base::span_from_ref(*email_origin));
  }

  if (!globally_enabled || !enabled_for_page) {
    return nullptr;
  }

  return content::webid::EmailVerifier::GetOrCreateForFrame(rfh);
}

}  // namespace

void EmailVerifierDelegate::Verify(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    std::string email,
    const std::string& nonce,
    const content::webid::EmailVerifier::Result& result) {
  content::RenderFrameHost* rfh = FindRenderFrameHostByToken(
      *static_cast<ContentAutofillClient&>(manager->client()).web_contents(),
      email_field_id.frame_token);
  content::webid::EmailVerifier* verifier = GetOrCreateEmailVerifier(
      manager->client(), rfh, GetOriginFromEmail(email));
  if (!verifier) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kVerifierUnavailable);
    return;
  }

  verifier->Verify(
      result, nonce,
      base::BindOnce(&EmailVerifierDelegate::OnVerificationResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), manager, email_field_id,
                     email, result.issuer_site));
}

void EmailVerifierDelegate::OnVerificationResponseReceived(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    std::string email,
    net::SchemefulSite issuer_site,
    std::optional<std::string> token,
    blink::mojom::EmailVerificationRequestResult status,
    base::TimeDelta verify_duration) {
  auto it = pending_request_metrics_.find(email_field_id);
  if (it == pending_request_metrics_.end()) {
    // Navigation already completed this flow and recorded
    // kPageNavigatedDuringVerification.
    return;
  }
  RequestMetrics& metrics = it->second;
  metrics.verify_status = status;
  metrics.verify_duration = verify_duration;

  if (!manager) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kManagerDestroyed);
    return;
  }
  if (manager->driver().GetLifecycleState() !=
      AutofillDriver::LifecycleState::kActive) {
    // Dismiss the first-run permission popup if it was kept open with a
    // loading spinner, since the frame is no longer active.
    manager->client().HideEmailVerificationPopup();
    NotifyFlowCompleted(email_field_id, EvpAutofillFlowResult::kDriverInactive);
    return;
  }
  if (!token) {
    // Dismiss the first-run permission prompt popup on verification failure
    // so that the in-button loading spinner does not linger, and display the
    // error toast.
    manager->client().HideEmailVerificationPopup();
    manager->client().ShowEmailVerificationErrorToast();
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kVerificationFailed);
    return;
  }
  // On successful verification, dismiss the first-run permission popup if
  // showing and display the email verified toast.
  manager->client().HideEmailVerificationPopup();
  manager->client().ShowEmailVerifiedToast(issuer_site.GetURL());
  issuers_[email_field_id] = issuer_site.GetURL();
  manager->driver().SendEmailVerificationToken(email_field_id, email, *token);
  NotifyFlowCompleted(email_field_id,
                      EvpAutofillFlowResult::kTokenSentToRenderer);
}

void EmailVerifierDelegate::OnEmailVerificationDecision(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    std::string email,
    std::string nonce,
    content::webid::EmailVerifier::Result result,
    AutofillClient::EmailVerificationPermissionUiStatus ui_status) {
  auto it = pending_request_metrics_.find(email_field_id);
  if (it == pending_request_metrics_.end()) {
    // Navigation already completed this flow and recorded
    // kPageNavigatedDuringVerification.
    return;
  }
  RequestMetrics& metrics = it->second;
  metrics.permission_ui_status = ui_status;

  if (!manager) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kManagerDestroyed);
    return;
  }
  if (manager->driver().GetLifecycleState() !=
      AutofillDriver::LifecycleState::kActive) {
    NotifyFlowCompleted(email_field_id, EvpAutofillFlowResult::kDriverInactive);
    return;
  }

  const std::string normalized_email = base::ToLowerASCII(email);
  PrefService* prefs = manager->client().GetPrefs();
  switch (ui_status) {
    case AutofillClient::EmailVerificationPermissionUiStatus::kAllowed: {
      if (prefs) {
        // Remember that the user allows this email address.
        ScopedDictPrefUpdate update(prefs,
                                    prefs::kAutofillEmailVerificationState);
        base::DictValue email_dict;
        if (const base::DictValue* existing =
                prefs->GetDict(prefs::kAutofillEmailVerificationState)
                    .FindDict(normalized_email)) {
          email_dict = existing->Clone();
        }
        email_dict.Set("allowed", true);
        email_dict.Set("issuer_site", result.issuer_site.Serialize());
        email_dict.Set("timestamp", base::TimeToValue(base::Time::Now()));
        update->Set(normalized_email, std::move(email_dict));
      }

      Verify(manager, email_field_id, email, nonce, result);

      const std::string strike_id =
          GetEmailVerificationStrikeDatabaseId(normalized_email);
      // User acceptance clears any accumulated strikes across both prompt
      // decline and not-signed-in strike databases for this email address.
      if (EmailVerificationStrikeDatabase* strike_db =
              GetEmailVerificationStrikeDatabase()) {
        strike_db->ClearStrikes(strike_id);
      }
      if (EmailVerificationNotSignedInStrikeDatabase* not_signed_in_strike_db =
              GetEmailVerificationNotSignedInStrikeDatabase()) {
        not_signed_in_strike_db->ClearStrikes(strike_id);
      }
      break;
    }
    case AutofillClient::EmailVerificationPermissionUiStatus::kDeclined: {
      if (EmailVerificationStrikeDatabase* strike_db =
              GetEmailVerificationStrikeDatabase()) {
        strike_db->AddStrike(
            GetEmailVerificationStrikeDatabaseId(normalized_email));
      }
      NotifyFlowCompleted(email_field_id,
                          EvpAutofillFlowResult::kUserDeclinedPermissionPrompt);
      break;
    }
    case AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted:
    case AutofillClient::EmailVerificationPermissionUiStatus::kNavigation:
    case AutofillClient::EmailVerificationPermissionUiStatus::kTabGone:
    case AutofillClient::EmailVerificationPermissionUiStatus::kWidgetChanged:
    case AutofillClient::EmailVerificationPermissionUiStatus::
        kOverlappingPrompt:
    case AutofillClient::EmailVerificationPermissionUiStatus::kOther:
    case AutofillClient::EmailVerificationPermissionUiStatus::
        kViewDestroyedDirectly: {
      NotifyFlowCompleted(email_field_id,
                          EvpAutofillFlowResult::kUserIgnoredPermissionPrompt);
      break;
    }
  }
}

void EmailVerifierDelegate::OnIsVerifiable(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    gfx::RectF email_field_bounds,
    std::u16string email,
    std::string nonce,
    bool already_allowed,
    std::optional<content::webid::EmailVerifier::Result> result,
    blink::mojom::EmailVerificationRequestResult status,
    base::TimeDelta is_verifiable_duration) {
  auto it = pending_request_metrics_.find(email_field_id);
  if (it == pending_request_metrics_.end()) {
    // Navigation already completed this flow and recorded
    // kPageNavigatedDuringCheckIfVerifiable.
    return;
  }
  RequestMetrics& metrics = it->second;
  // Prevent double logging
  if (metrics.is_verifiable_status) {
    return;
  }
  metrics.is_verifiable_status = status;
  metrics.is_verifiable_duration = is_verifiable_duration;

  if (!manager) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kManagerDestroyed);
    return;
  }
  if (manager->driver().GetLifecycleState() !=
      AutofillDriver::LifecycleState::kActive) {
    NotifyFlowCompleted(email_field_id, EvpAutofillFlowResult::kDriverInactive);
    return;
  }

  const std::string email_utf8 = base::UTF16ToUTF8(email);
  const std::string normalized_email = base::ToLowerASCII(email_utf8);

  if (!result) {
    if (status ==
            blink::mojom::EmailVerificationRequestResult::kUserLoggedOut &&
        GetEmailVerificationNotSignedInStrikeDatabase()) {
      // Record a strike when the email verification check determines that
      // the user is not signed in with the given email address. This enforces
      // the 3-strike rate-limiting mechanism for unauthenticated emails.
      GetEmailVerificationNotSignedInStrikeDatabase()->AddStrike(
          GetEmailVerificationStrikeDatabaseId(normalized_email));
    }
    NotifyFlowCompleted(email_field_id, EvpAutofillFlowResult::kNotVerifiable);
    return;
  }

  if (already_allowed) {
    if (EmailVerificationNotSignedInStrikeDatabase* not_signed_in_strike_db =
            GetEmailVerificationNotSignedInStrikeDatabase()) {
      // Clear past not-signed-in strikes since the email is already permitted
      // and now successfully verified as verifiable.
      not_signed_in_strike_db->ClearStrikes(
          GetEmailVerificationStrikeDatabaseId(normalized_email));
    }
    // For subsequent-run requests where permission was already granted
    // previously, the permission prompt popup is bypassed. Display a loading
    // toast to inform the user that background email verification is in
    // progress while the token request is in flight.
    manager->client().ShowEmailVerificationLoadingToast();
    Verify(manager, email_field_id, email_utf8, nonce, *result);
    return;
  }

  net::SchemefulSite issuer_site = result->issuer_site;
  manager->client().ShowEmailVerificationPopup(
      email_field_bounds, issuer_site, base::UTF8ToUTF16(normalized_email),
      base::BindOnce(&EmailVerifierDelegate::OnEmailVerificationDecision,
                     weak_ptr_factory_.GetWeakPtr(), manager, email_field_id,
                     email_utf8, nonce, std::move(*result)));
}


EmailVerifierDelegate::EmailVerifierDelegate(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {
  AddObserver(&metrics_observer_);
  observation_.Observe(client);
  if (auto* content_client = static_cast<ContentAutofillClient*>(client)) {
    Observe(content_client->web_contents());
  }
}

EmailVerifierDelegate::~EmailVerifierDelegate() {
  RemoveObserver(&metrics_observer_);
}

EmailVerificationStrikeDatabase*
EmailVerifierDelegate::GetEmailVerificationStrikeDatabase() {
  if (!email_verification_strike_db_ && client_->GetStrikeDatabase()) {
    email_verification_strike_db_.emplace(client_->GetStrikeDatabase());
  }
  return base::OptionalToPtr(email_verification_strike_db_);
}

EmailVerificationNotSignedInStrikeDatabase*
EmailVerifierDelegate::GetEmailVerificationNotSignedInStrikeDatabase() {
  if (!email_verification_not_signed_in_strike_db_ &&
      client_->GetStrikeDatabase()) {
    email_verification_not_signed_in_strike_db_.emplace(
        client_->GetStrikeDatabase());
  }
  return base::OptionalToPtr(email_verification_not_signed_in_strike_db_);
}

void EmailVerifierDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EmailVerifierDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

EmailVerifierDelegate::MetricsObserver::MetricsObserver() = default;
EmailVerifierDelegate::MetricsObserver::~MetricsObserver() = default;

void EmailVerifierDelegate::MetricsObserver::OnFlowCompleted(
    const RequestMetrics& metrics) {
  if (metrics.ukm_source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::Blink_EmailVerificationProtocol builder(metrics.ukm_source_id);
  if (metrics.autofill_flow_result) {
    builder.SetAutofill_FlowResult(
        static_cast<int64_t>(*metrics.autofill_flow_result));
  }
  if (metrics.permission_ui_status) {
    builder.SetPermissionUi_Status(
        static_cast<int64_t>(*metrics.permission_ui_status));
  }
  if (metrics.is_verifiable_status) {
    builder.SetStatus_IsVerifiable(
        static_cast<int64_t>(*metrics.is_verifiable_status));
  }
  if (metrics.is_verifiable_duration) {
    builder.SetTiming_IsVerifiable(ukm::GetExponentialBucketMinForUserTiming(
        metrics.is_verifiable_duration->InMilliseconds()));
  }
  if (metrics.verify_status) {
    builder.SetStatus_Verify(static_cast<int64_t>(*metrics.verify_status));
  }
  if (metrics.verify_duration) {
    builder.SetTiming_Verify(ukm::GetExponentialBucketMinForUserTiming(
        metrics.verify_duration->InMilliseconds()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void EmailVerifierDelegate::NotifyFlowCompleted(FieldGlobalId field_id,
                                                EvpAutofillFlowResult result) {
  base::UmaHistogramEnumeration("Blink.Evp.Autofill.FlowResult", result);

  if (auto it = pending_request_metrics_.find(field_id);
      it != pending_request_metrics_.end()) {
    RequestMetrics metrics = std::move(it->second);
    pending_request_metrics_.erase(it);
    metrics.autofill_flow_result = result;
    for (Observer& observer : observers_) {
      observer.OnFlowCompleted(metrics);
    }
  }
}

void EmailVerifierDelegate::CancelPendingRequests() {
  if (!pending_request_metrics_.empty()) {
    // Create a copy of keys and flow results because NotifyFlowCompleted
    // erases from the map.
    std::vector<std::pair<FieldGlobalId, EvpAutofillFlowResult>>
        pending_requests;
    pending_requests.reserve(pending_request_metrics_.size());
    for (const auto& [email_field_id, metrics] : pending_request_metrics_) {
      EvpAutofillFlowResult flow_result =
          metrics.is_verifiable_status.has_value()
              ? EvpAutofillFlowResult::kPageNavigatedDuringVerification
              : EvpAutofillFlowResult::kPageNavigatedDuringCheckIfVerifiable;
      pending_requests.emplace_back(email_field_id, flow_result);
    }
    for (const auto& [email_field_id, flow_result] : pending_requests) {
      NotifyFlowCompleted(email_field_id, flow_result);
    }
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EmailVerifierDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    if (!navigation_handle->IsSameDocument()) {
      CancelPendingRequests();
    }
    // `HasCommitted` returns true even for same document commits, e.g.
    // if the state is cleared on pushState() or #anchor navigations.
    // We clear the issuers_ map on these navigations too.
    issuers_.clear();
    last_focused_field_ = std::nullopt;
    last_verified_values_.clear();
  }
}

void EmailVerifierDelegate::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId trigger_field_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&,
    const FillingPayload& filling_payload) {
  if (!base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol)) {
    return;
  }

  const AutofillProfile* const* profile =
      std::get_if<const AutofillProfile*>(&filling_payload);
  if (action_persistence != mojom::ActionPersistence::kFill || !profile) {
    return;
  }

  auto [form, triggering_email_field] =
      manager.FindFormAndField(form_id, trigger_field_id);
  if (!form || !triggering_email_field ||
      triggering_email_field->autofilled_type() != EMAIL_ADDRESS) {
    return;
  }

  // TODO(crbug.com/446288895): Currently, when filling a form, the browser
  // notifies observers via `OnFillOrPreviewForm()` **before** it sends the fill
  // request to the renderer and **before** it updates its own cache with the
  // newly filled values. Because of this timing, if we try to read
  // `email_field->value()` inside the callback, we will get the old value
  // (before filling), not the new email address. So, we extract it manually
  // from the profile instead.
  // We should introduce `OnFilledOrPreviewedForm` and move the notification to
  // a later stage (specifically after the renderer confirms the fill and the
  // browser updates its cache) so we can use the `email_field->value()`
  // instead.
  std::u16string email = (*profile)->GetRawInfo(EMAIL_ADDRESS);
  QueryNonce(manager, *triggering_email_field, email);
}

void EmailVerifierDelegate::OnFillOrPreviewField(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id,
    mojom::ActionPersistence action_persistence,
    const std::u16string& value,
    std::optional<FieldType> field_type_used) {
  if (!base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol)) {
    return;
  }

  if (action_persistence != mojom::ActionPersistence::kFill) {
    return;
  }

  auto [form, triggering_email_field] =
      manager.FindFormAndField(form_id, field_id);
  if (!form || !triggering_email_field ||
      (field_type_used != EMAIL_ADDRESS &&
       triggering_email_field->Type().GetAddressType() != EMAIL_ADDRESS)) {
    return;
  }

  QueryNonce(manager, *triggering_email_field, value);
}

void EmailVerifierDelegate::OnBeforeFormWithEmailVerificationTokenSubmitted(
    AutofillManager& manager,
    const FormData& form,
    const FieldGlobalId& email_field_id) {
  if (manager.driver().GetLifecycleState() !=
      AutofillDriver::LifecycleState::kActive) {
    return;
  }
  if (auto it = issuers_.find(email_field_id); it != issuers_.end()) {
    issuers_.erase(it);
    // Record form submission metrics for verified email fields.
    base::UmaHistogramBoolean("Blink.Evp.Autofill.FormSubmitted", true);
    ukm::builders::Blink_EmailVerificationProtocol_FormSubmission(
        manager.driver().GetPageUkmSourceId())
        .SetAutofill_FormSubmitted(true)
        .Record(ukm::UkmRecorder::Get());
  }
}

void EmailVerifierDelegate::OnBeforeFormSubmitted(AutofillManager& manager,
                                                  const FormData& form) {
  CancelPendingRequests();
}

void EmailVerifierDelegate::OnAfterFocusOnFormField(AutofillManager& manager,
                                                    FormGlobalId form_id,
                                                    FieldGlobalId field_id) {
  if (last_focused_field_ && *last_focused_field_ != field_id) {
    OnFieldLostFocus(manager, *last_focused_field_);
  }
  last_focused_field_ = field_id;
}

void EmailVerifierDelegate::OnAfterFocusOnNonFormField(
    AutofillManager& manager) {
  if (last_focused_field_) {
    OnFieldLostFocus(manager, *last_focused_field_);
    last_focused_field_ = std::nullopt;
  }
}

void EmailVerifierDelegate::OnFieldLostFocus(AutofillManager& manager,
                                             const FieldGlobalId& field_id) {
  if (!base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol)) {
    return;
  }
  const FormStructure* form = manager.FindCachedFormById(field_id);
  if (!form) {
    return;
  }
  const AutofillField* email_field = form->GetFieldById(field_id);
  if (!email_field) {
    return;
  }

  // Check 1: Type is EMAIL_ADDRESS
  if (email_field->Type().GetAddressType() != EMAIL_ADDRESS) {
    return;
  }

  // Check 2: User Edit (last_modifier is kUser)
  if (email_field->last_modifier() != FieldModifier::kUser) {
    return;
  }

  // Check 3: Value validation
  const std::u16string& value = email_field->value();
  if (!IsValidEmailAddress(value)) {
    return;
  }

  std::string lowercase_value = base::ToLowerASCII(base::UTF16ToUTF8(value));

  // Check 4: Deduplication (LRU Cache)
  auto it = std::ranges::find_if(last_verified_values_, [&](const auto& pair) {
    return pair.first == field_id;
  });
  if (it != last_verified_values_.end() && it->second == lowercase_value) {
    // Value is same, deduplicate. Move to back to update LRU status.
    auto pair = *it;
    last_verified_values_.erase(it);
    last_verified_values_.push_back(pair);
    return;
  }

  // Value is different or new. Update cache and trigger.
  if (it != last_verified_values_.end()) {
    last_verified_values_.erase(it);
  }
  last_verified_values_.push_back({field_id, lowercase_value});
  if (last_verified_values_.size() > 5) {
    last_verified_values_.erase(last_verified_values_.begin());
  }

  QueryNonce(manager, *email_field, value);
}

void EmailVerifierDelegate::QueryNonce(AutofillManager& manager,
                                       const AutofillField& email_field,
                                       const std::u16string& email_value) {
  if (!base::FeatureList::IsEnabled(::features::kEmailVerificationProtocol)) {
    return;
  }

  manager.driver().GetNonceForEmailVerification(
      email_field.global_id(),
      base::BindOnce(&EmailVerifierDelegate::OnNonceReceived,
                     weak_ptr_factory_.GetWeakPtr(), manager.GetWeakPtr(),
                     email_field.global_id(), email_field.bounds(),
                     email_value));
}

void EmailVerifierDelegate::OnNonceReceived(
    base::WeakPtr<AutofillManager> manager,
    FieldGlobalId email_field_id,
    gfx::RectF email_field_bounds,
    std::u16string email_value,
    const std::optional<std::string>& nonce) {
  if (!manager) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kManagerDestroyed);
    return;
  }
  switch (manager->driver().GetLifecycleState()) {
    case AutofillDriver::LifecycleState::kInactive:
    case AutofillDriver::LifecycleState::kPendingReset:
    case AutofillDriver::LifecycleState::kPendingDeletion:
      NotifyFlowCompleted(email_field_id,
                          EvpAutofillFlowResult::kDriverInactive);
      return;
    case AutofillDriver::LifecycleState::kActive:
      break;
  }

  if (!nonce) {
    return;
  }

  TriggerVerification(*manager, email_field_id, email_field_bounds, email_value,
                      *nonce);
}

void EmailVerifierDelegate::TriggerVerification(AutofillManager& manager,
                                                FieldGlobalId email_field_id,
                                                gfx::RectF email_field_bounds,
                                                std::u16string email_value,
                                                const std::string& nonce) {
  pending_request_metrics_[email_field_id] = RequestMetrics();
  RequestMetrics& metrics = pending_request_metrics_[email_field_id];
  content::RenderFrameHost* rfh = FindRenderFrameHostByToken(
      *static_cast<ContentAutofillClient&>(manager.client()).web_contents(),
      email_field_id.frame_token);
  if (rfh) {
    metrics.ukm_source_id = rfh->GetPageUkmSourceId();
  }

  if (nonce.empty()) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kTokenFieldHasNoNonce);
    return;
  }

  const PrefService* prefs = manager.client().GetPrefs();
  if (!prefs || !prefs->GetBoolean(prefs::kAutofillEmailVerificationEnabled)) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kUserPrefDisabled);
    return;
  }

  const std::string email = base::UTF16ToUTF8(email_value);
  const std::string normalized_email = base::ToLowerASCII(email);

  content::webid::EmailVerifier* verifier = GetOrCreateEmailVerifier(
      manager.client(), rfh, GetOriginFromEmail(email));
  if (!verifier) {
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kVerifierUnavailable);
    return;
  }

  // Record that the page has triggered the email verification protocol and
  // the feature is active. We record this use counter when verification is
  // first triggered on a page that successfully opted into the origin trial.
  // Note that the use counter shouldn't depend on the status of strike or
  // verify. It only represents whether the API is triggered on the website.
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      rfh, blink::mojom::WebFeature::kEmailVerificationProtocol);

  const std::string strike_id =
      GetEmailVerificationStrikeDatabaseId(normalized_email);
  EmailVerificationStrikeDatabase* strike_db =
      GetEmailVerificationStrikeDatabase();
  if (strike_db && strike_db->ShouldBlockFeature(strike_id)) {
    // If the email has reached the strike limit for user prompt declines,
    // suppress the verification flow early.
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kStrikeDatabaseBlock);
    return;
  }
  EmailVerificationNotSignedInStrikeDatabase* not_signed_in_strike_db =
      GetEmailVerificationNotSignedInStrikeDatabase();
  if (not_signed_in_strike_db &&
      not_signed_in_strike_db->ShouldBlockFeature(strike_id)) {
    // If the email has reached the strike limit for not-signed-in attempts,
    // suppress the verification flow early.
    NotifyFlowCompleted(email_field_id,
                        EvpAutofillFlowResult::kNotSignedInStrikeDatabaseBlock);
    return;
  }

  const base::DictValue& state =
      prefs->GetDict(prefs::kAutofillEmailVerificationState);
  const base::DictValue* email_data = state.FindDict(normalized_email);
  const bool auto_grant = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAutoGrantEmailVerificationPermissionForTesting);
  const bool already_allowed =
      auto_grant ||
      (email_data && email_data->FindBool("allowed").value_or(false));

  // TODO(crbug.com/555673135): Check if the on_dns_resolved callback is still
  // needed in EmailVerifier::CheckIfVerifiable now that in-element indicators
  // are removed.
  verifier->CheckIfVerifiable(
      email, base::DoNothing(),
      base::BindOnce(&EmailVerifierDelegate::OnIsVerifiable,
                     weak_ptr_factory_.GetWeakPtr(), manager.GetWeakPtr(),
                     email_field_id, email_field_bounds, email_value, nonce,
                     already_allowed));
}

}  // namespace autofill
