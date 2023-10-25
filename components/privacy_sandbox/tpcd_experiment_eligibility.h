// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TPCD_EXPERIMENT_ELIGIBILITY_H_
#define COMPONENTS_PRIVACY_SANDBOX_TPCD_EXPERIMENT_ELIGIBILITY_H_

namespace privacy_sandbox {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
class TpcdExperimentEligibility {
 public:
  enum class Reason {
    kEligible = 0,
    k3pCookiesBlocked = 1,
    kHasNotSeenNotice = 2,
    kNewUser = 3,
    kEnterpriseUser = 4,
    kPwaOrTwaInstalled = 5,  // Android only
    kForcedEligible = 6,
    kMaxValue = kForcedEligible,
  };

  explicit TpcdExperimentEligibility(Reason reason) : reason_(reason) {}

  TpcdExperimentEligibility(const TpcdExperimentEligibility& other) = default;
  TpcdExperimentEligibility& operator=(const TpcdExperimentEligibility& other) =
      default;

  bool is_eligible() const {
    return reason_ == Reason::kEligible || reason_ == Reason::kForcedEligible;
  }
  Reason reason() const { return reason_; }

 private:
  Reason reason_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TPCD_EXPERIMENT_ELIGIBILITY_H_
