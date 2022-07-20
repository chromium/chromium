// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/portal_detector/network_portal_detector_strategy.h"

#include <memory>

#include "base/logging.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

namespace {

const NetworkState* DefaultNetwork() {
  return NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
}

class LoginScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kBaseAttemptTimeoutSec = 5;
  static const int kMaxAttemptTimeoutSec = 30;

  explicit LoginScreenStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}

  LoginScreenStrategy(const LoginScreenStrategy&) = delete;
  LoginScreenStrategy& operator=(const LoginScreenStrategy&) = delete;

  ~LoginScreenStrategy() override = default;

 protected:
  // PortalDetectorStrategy overrides:
  StrategyId Id() const override { return STRATEGY_ID_LOGIN_SCREEN; }
  base::TimeDelta GetNextAttemptTimeoutImpl() override {
    if (DefaultNetwork() && delegate_->NoResponseResultCount() != 0) {
      int timeout = kMaxAttemptTimeoutSec;
      if (kMaxAttemptTimeoutSec / (delegate_->NoResponseResultCount() + 1) >
          kBaseAttemptTimeoutSec) {
        timeout =
            kBaseAttemptTimeoutSec * (delegate_->NoResponseResultCount() + 1);
      }
      return base::Seconds(timeout);
    }
    return base::Seconds(kBaseAttemptTimeoutSec);
  }
};

class ErrorScreenStrategy : public PortalDetectorStrategy {
 public:
  static const int kAttemptTimeoutSec = 15;

  explicit ErrorScreenStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}

  ErrorScreenStrategy(const ErrorScreenStrategy&) = delete;
  ErrorScreenStrategy& operator=(const ErrorScreenStrategy&) = delete;

  ~ErrorScreenStrategy() override = default;

 protected:
  // PortalDetectorStrategy overrides:
  StrategyId Id() const override { return STRATEGY_ID_ERROR_SCREEN; }
  base::TimeDelta GetNextAttemptTimeoutImpl() override {
    return base::Seconds(kAttemptTimeoutSec);
  }
};

class SessionStrategy : public PortalDetectorStrategy {
 public:
  static const int kMaxFastAttempts = 3;
  static const int kFastAttemptTimeoutSec = 3;
  static const int kSlowAttemptTimeoutSec = 5;

  explicit SessionStrategy(PortalDetectorStrategy::Delegate* delegate)
      : PortalDetectorStrategy(delegate) {}

  SessionStrategy(const SessionStrategy&) = delete;
  SessionStrategy& operator=(const SessionStrategy&) = delete;

  ~SessionStrategy() override = default;

 protected:
  StrategyId Id() const override { return STRATEGY_ID_SESSION; }
  base::TimeDelta GetNextAttemptTimeoutImpl() override {
    int timeout;
    if (delegate_->NoResponseResultCount() < kMaxFastAttempts)
      timeout = kFastAttemptTimeoutSec;
    else
      timeout = kSlowAttemptTimeoutSec;
    return base::Seconds(timeout);
  }
};

}  // namespace

// PortalDetectorStrategy::Delegate --------------------------------------------

PortalDetectorStrategy::Delegate::~Delegate() = default;

// PortalDetectorStrategy -----------------------------------------------------

// static
base::TimeDelta PortalDetectorStrategy::delay_till_next_attempt_for_testing_;

// static
bool PortalDetectorStrategy::delay_till_next_attempt_for_testing_initialized_ =
    false;

// static
base::TimeDelta PortalDetectorStrategy::next_attempt_timeout_for_testing_;

// static
bool PortalDetectorStrategy::next_attempt_timeout_for_testing_initialized_ =
    false;

PortalDetectorStrategy::PortalDetectorStrategy(Delegate* delegate)
    : delegate_(delegate) {
  // First |policy_.num_errors_to_ignore| attempts with the same
  // result are performed with |policy_.initial_delay_ms| between
  // them. Delay before every consecutive attempt is multplied by
  // |policy_.multiply_factor_|. Also, |policy_.jitter_factor| is used
  // for each delay.
  policy_.num_errors_to_ignore = 3;
  policy_.initial_delay_ms = 600;
  policy_.multiply_factor = 2.0;
  policy_.jitter_factor = 0.3;
  policy_.maximum_backoff_ms = 2 * 60 * 1000;
  policy_.entry_lifetime_ms = -1;
  policy_.always_use_initial_delay = true;
  backoff_entry_ = std::make_unique<net::BackoffEntry>(&policy_, delegate_);
}

PortalDetectorStrategy::~PortalDetectorStrategy() = default;

// static
std::unique_ptr<PortalDetectorStrategy> PortalDetectorStrategy::CreateById(
    StrategyId id,
    Delegate* delegate) {
  switch (id) {
    case STRATEGY_ID_LOGIN_SCREEN:
      return std::make_unique<LoginScreenStrategy>(delegate);
    case STRATEGY_ID_ERROR_SCREEN:
      return std::make_unique<ErrorScreenStrategy>(delegate);
    case STRATEGY_ID_SESSION:
      return std::make_unique<SessionStrategy>(delegate);
  }
  NOTREACHED();
  return nullptr;
}

base::TimeDelta PortalDetectorStrategy::GetDelayTillNextAttempt() {
  if (delay_till_next_attempt_for_testing_initialized_)
    return delay_till_next_attempt_for_testing_;
  return backoff_entry_->GetTimeUntilRelease();
}

base::TimeDelta PortalDetectorStrategy::GetNextAttemptTimeout() {
  if (next_attempt_timeout_for_testing_initialized_)
    return next_attempt_timeout_for_testing_;
  return GetNextAttemptTimeoutImpl();
}

void PortalDetectorStrategy::Reset() {
  backoff_entry_->Reset();
}

void PortalDetectorStrategy::SetPolicyAndReset(
    const net::BackoffEntry::Policy& policy) {
  policy_ = policy;
  backoff_entry_ = std::make_unique<net::BackoffEntry>(&policy_, delegate_);
}

void PortalDetectorStrategy::OnDetectionCompleted() {
  backoff_entry_->InformOfRequest(false);
}

}  // namespace ash
