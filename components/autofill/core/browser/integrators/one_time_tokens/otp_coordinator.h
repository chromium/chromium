// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_COORDINATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_COORDINATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_delivery.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_target_field_identifier.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace autofill {

class AutofillClient;
class BrowserAutofillManager;

// Tab-scoped coordinator owned by `AutofillClient`.
// Starts and stops `OtpSource` instances as OTP fields appear and disappear
// and maintains per-field `OtpDelivery` state machines.
class OtpCoordinator : public OtpSource::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual BrowserAutofillManager& GetBrowserAutofillManager() const = 0;
    virtual void OnOtpAvailable(const OtpSuggestionData& data) = 0;
  };

  // `client` must be non-null and outlive this coordinator.
  explicit OtpCoordinator(AutofillClient* client);
  OtpCoordinator(const OtpCoordinator&) = delete;
  OtpCoordinator& operator=(const OtpCoordinator&) = delete;
  ~OtpCoordinator() override;

  // Starts or renews listening for OTPs for `target`.
  void StartListeningForOtp(const OtpTarget& target);

  // `observer` must be non-null and is retained until `UnregisterObserver()` is
  // called or `this` is destroyed.
  void RegisterObserver(Observer* observer);
  void UnregisterObserver(Observer* observer);

  // OtpSource::Observer:
  void OnSourceCandidateAvailable(
      std::optional<OtpTarget> source_target,
      base::expected<OtpCandidate, one_time_tokens::OneTimeTokenRetrievalError>
          candidate_or_error) override;

 private:
  friend class OtpCoordinatorTestApi;

  Observer* FindObserverForFrame(LocalFrameToken frame);
  bool IsTargetActiveAndEligible(const OtpTarget& target) const;

  const raw_ref<AutofillClient> client_;
  OtpTargetFieldIdentifier target_field_identifier_;
  base::flat_map<FieldGlobalId, OtpDelivery> deliveries_;
  std::vector<std::unique_ptr<OtpSource>> sources_;
  base::ObserverList<Observer> observers_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_COORDINATOR_H_
