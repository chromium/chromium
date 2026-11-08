// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_COORDINATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_COORDINATOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_delivery.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_security_gateway.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion_dropdown_dispatcher.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_target_field_identifier.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "url/origin.h"

namespace one_time_tokens {
class OneTimeToken;
}  // namespace one_time_tokens

namespace autofill {

class AutofillClient;
class BrowserAutofillManager;
class FormData;

// Tab-scoped coordinator owned by `AutofillClient`.
// Starts and stops `OtpSource` instances as OTP fields appear and disappear,
// maintains per-field `OtpDelivery` state machines, validates tokens via
// `OtpSecurityGateway`, and dispatches proactive UI triggers via
// `OtpSuggestionDropdownDispatcher`.
//
// While active in a tab, `OtpCoordinator`'s `OtpSecurityGateway` must be the
// sole caller of the single-flight `AutofillClient::GetOtpPhishGuardDelegate()`.
class OtpCoordinator : public OtpSource::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual BrowserAutofillManager& GetBrowserAutofillManager() const = 0;
    virtual void OnOtpAvailable(const OtpSuggestionData& data) = 0;
    virtual void TriggerSuggestions(FieldGlobalId field_id) = 0;
  };

  // `client` must be non-null and outlive this coordinator.
  explicit OtpCoordinator(AutofillClient* client);
  OtpCoordinator(const OtpCoordinator&) = delete;
  OtpCoordinator& operator=(const OtpCoordinator&) = delete;
  ~OtpCoordinator() override;

  // Starts or renews listening for OTPs for `target`.
  void StartListeningForOtp(const OtpTarget& target);

  // Starts or renews listening for OTPs across all eligible targets in the tab.
  void StartListeningForEligibleTargets();

  // Drops active OTP deliveries for `form_id` whose fields are not in `except`
  // and stops sources for any origin that has no remaining active targets.
  void StopListeningForForm(FormGlobalId form_id,
                            const base::flat_set<FieldGlobalId>& except = {});

  // Returns verified, unused OTPs available for `target`.
  std::vector<one_time_tokens::OneTimeToken> GetAvailableTokens(
      const OtpTarget& target) const;

  // `observer` must be non-null and is retained until `UnregisterObserver()` is
  // called or `this` is destroyed.
  void RegisterObserver(Observer* observer);
  void UnregisterObserver(Observer* observer);

  // Drops active deliveries owned by or hosted in `frame` when `frame`
  // transitions away from active lifecycle state (e.g. into BFCache or pending
  // deletion).
  void OnFrameBecameInactive(LocalFrameToken frame);

  // Updates tracked focus state when focus moves to `(form_id, field_id)` in
  // `frame`, or to a non-form element when `field_id` is `std::nullopt`.
  void OnFocusChanged(LocalFrameToken frame,
                      std::optional<FormGlobalId> form_id,
                      std::optional<FieldGlobalId> field_id);

  // Updates tab visibility and retries deferred proactive triggers when the tab
  // becomes visible while the browser window is focused.
  void OnTabVisibilityChanged(bool is_visible);

  // Updates browser window focus and retries deferred proactive triggers when
  // the window becomes focused while the tab is visible.
  void OnWindowFocusChanged(bool is_focused);

  // Records that the user typed in a text field after OTP detection,
  // suppressing subsequent proactive dropdown triggers for the current session.
  void OnTextFieldValueChanged();

  // Records that an OTP suggestion with `shown_value` was displayed on `field`
  // so proactive triggers do not re-open the dropdown for the same token on
  // that form.
  void OnOtpSuggestionsShown(FieldGlobalId field,
                             const std::string& shown_value);

  // Marks `filled_value` as consumed across sources and deliveries for
  // `target.origin`, and suppresses re-offering of matching or older tokens on
  // `target.origin` until expiration.
  void OnOtpFilled(const OtpTarget& target, const std::string& filled_value);

  // Consumes any non-empty OTP field value on `form` across sources and
  // deliveries for the field's origin, and drops the form's deliveries.
  void OnOtpFormSubmitted(const FormData& form);

  // OtpSource::Observer:
  void OnSourceCandidateAvailable(
      std::optional<OtpTarget> source_target,
      base::expected<OtpCandidate, one_time_tokens::OneTimeTokenRetrievalError>
          candidate_or_error) override;

 private:
  friend class OtpCoordinatorTestApi;

  using SiteFrame = std::pair<url::Origin, LocalFrameToken>;

  struct UsedToken {
    OtpTokenIdentity identity;
    base::TimeTicks recorded_at;
  };

  static SiteFrame GetSiteFrame(const OtpTarget& target);
  static bool IsUsedTokenExpired(const UsedToken& used, base::TimeTicks now);

  void OnDeliveryValidated(uint64_t generation,
                           OtpTarget target,
                           OtpCandidate candidate,
                           OtpDeliveryPolicy::Decision decision);
  void TryProactiveTrigger(const OtpTarget& target,
                           const OtpCandidate& candidate);
  void RetryDeferredProactiveTriggers();
  void RecordFormSuggestionsShown(FormGlobalId form_id,
                                  const OtpTokenIdentity& identity);
  void DropDeliveries(base::FunctionRef<bool(const OtpDelivery&)> pred);
  void StopSourcesIfNoRemainingTargets(const url::Origin& origin);
  bool IsCandidateSuppressedByUsedTokens(const OtpTarget& target,
                                         const OtpCandidate& candidate) const;
  std::optional<OtpCandidate> GetAvailableCandidate(FieldGlobalId field) const;
  bool HasActiveDeliveryForSiteFrame(const SiteFrame& site_frame) const;
  Observer* FindObserverForFrame(LocalFrameToken frame);
  bool IsTargetActiveAndEligible(const OtpTarget& target) const;

  const raw_ref<AutofillClient> client_;
  OtpTargetFieldIdentifier target_field_identifier_;
  base::flat_map<FieldGlobalId, OtpDelivery> deliveries_;
  base::flat_map<url::Origin, std::vector<UsedToken>> used_tokens_per_origin_;
  base::flat_set<SiteFrame> phishing_blocked_site_frames_;
  base::flat_map<SiteFrame, OtpTokenIdentity> in_flight_validations_;
  std::vector<std::unique_ptr<OtpSource>> sources_;
  OtpSecurityGateway security_gateway_;
  OtpSuggestionDropdownDispatcher suggestion_dropdown_dispatcher_;
  base::ObserverList<Observer> observers_;

  bool is_tab_visible_ = true;
  bool is_window_focused_ = true;
  // Incremented whenever all active OTP deliveries are cleared so in-flight
  // `OnDeliveryValidated` callbacks from a previous listening session are
  // ignored.
  uint64_t listening_generation_ = 0;

  base::WeakPtrFactory<OtpCoordinator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_COORDINATOR_H_
