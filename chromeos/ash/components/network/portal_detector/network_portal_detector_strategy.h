// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"

// TODO(https://crbug.com/1164001): remove after moving to ash/.
namespace ash {
class NetworkPortalDetectorImplTest;
class NetworkPortalDetectorImplBrowserTest;
}  // namespace ash

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) PortalDetectorStrategy {
 public:
  enum StrategyId {
    STRATEGY_ID_LOGIN_SCREEN,
    STRATEGY_ID_ERROR_SCREEN,
    STRATEGY_ID_SESSION
  };

  class Delegate : public base::TickClock {
   public:
    ~Delegate() override;

    // Returns number of attempts in a row with NO RESPONSE result.
    // If last detection attempt has different result, returns 0.
    virtual int NoResponseResultCount() = 0;

    // Returns time when current attempt was started.
    virtual base::TimeTicks AttemptStartTime() = 0;
  };

  PortalDetectorStrategy(const PortalDetectorStrategy&) = delete;
  PortalDetectorStrategy& operator=(const PortalDetectorStrategy&) = delete;

  virtual ~PortalDetectorStrategy();

  // Lifetime of delegate must enclose lifetime of PortalDetectorStrategy.
  static std::unique_ptr<PortalDetectorStrategy> CreateById(StrategyId id,
                                                            Delegate* delegate);

  // Returns delay before next detection attempt. This delay is needed
  // to separate detection attempts in time.
  base::TimeDelta GetDelayTillNextAttempt();

  // Returns timeout for the next detection attempt.
  base::TimeDelta GetNextAttemptTimeout();

  virtual StrategyId Id() const = 0;

  // Resets strategy to the initial state.
  void Reset();

  const net::BackoffEntry::Policy& policy() const { return policy_; }

  // Resets strategy to the initial stater and sets custom policy.
  void SetPolicyAndReset(const net::BackoffEntry::Policy& policy);

  // Should be called when portal detection is completed and timeout before next
  // attempt should be adjusted.
  void OnDetectionCompleted();

 protected:
  // Lifetime of delegate must enclose lifetime of PortalDetectorStrategy.
  explicit PortalDetectorStrategy(Delegate* delegate);

  // Interface for subclasses:
  virtual base::TimeDelta GetNextAttemptTimeoutImpl() = 0;

  Delegate* delegate_;
  net::BackoffEntry::Policy policy_;
  std::unique_ptr<net::BackoffEntry> backoff_entry_;

 private:
  friend class ash::NetworkPortalDetectorImplTest;
  friend class ash::NetworkPortalDetectorImplBrowserTest;

  static void set_delay_till_next_attempt_for_testing(
      const base::TimeDelta& timeout) {
    delay_till_next_attempt_for_testing_ = timeout;
    delay_till_next_attempt_for_testing_initialized_ = true;
  }

  static void set_next_attempt_timeout_for_testing(
      const base::TimeDelta& timeout) {
    next_attempt_timeout_for_testing_ = timeout;
    next_attempt_timeout_for_testing_initialized_ = true;
  }

  static void reset_fields_for_testing() {
    delay_till_next_attempt_for_testing_initialized_ = false;
    next_attempt_timeout_for_testing_initialized_ = false;
  }

  // Test delay before detection attempt, used by unit tests.
  static base::TimeDelta delay_till_next_attempt_for_testing_;

  // True when |min_time_between_attempts_for_testing_| is initialized.
  static bool delay_till_next_attempt_for_testing_initialized_;

  // Test timeout for a detection attempt, used by unit tests.
  static base::TimeDelta next_attempt_timeout_for_testing_;

  // True when |next_attempt_timeout_for_testing_| is initialized.
  static bool next_attempt_timeout_for_testing_initialized_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::PortalDetectorStrategy;
}

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PORTAL_DETECTOR_NETWORK_PORTAL_DETECTOR_STRATEGY_H_
