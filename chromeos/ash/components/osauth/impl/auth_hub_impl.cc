// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_vector_lifecycle.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/string_utils.h"

namespace ash {

AuthHubImpl::AuthHubImpl() {
  mode_lifecycle_ = std::make_unique<AuthHubModeLifecycle>(this);
}

AuthHubImpl::~AuthHubImpl() = default;

void AuthHubImpl::InitializeForMode(AuthHubMode target) {
  if (vector_lifecycle_ && !vector_lifecycle_->IsIdle()) {
    target_mode_ = target;
    // Eventually, after the current attempt gets canceled, `OnIdle()` will be
    // triggered, which then switches the mode to `target_mode_`.
    vector_lifecycle_->CancelAttempt();
    return;
  }
  mode_lifecycle_->SwitchToMode(target);
}

void AuthHubImpl::EnsureInitialized(base::OnceClosure on_initialized) {
  if (mode_lifecycle_->IsReady()) {
    std::move(on_initialized).Run();
    return;
  }
  on_initialized_listeners_.AddUnsafe(std::move(on_initialized));
}

void AuthHubImpl::StartAuthentication(AccountId account_id,
                                      AuthPurpose purpose,
                                      AuthAttemptConsumer* consumer) {
  if (!PurposeMatchesMode(purpose, mode_lifecycle_->GetCurrentMode())) {
    LOG(ERROR) << "Attempt for " << purpose
               << " rejected due to incorrect mode "
               << mode_lifecycle_->GetCurrentMode();
    consumer->OnUserAuthAttemptRejected();
    return;
  }

  CHECK(vector_lifecycle_);
  AuthAttemptVector attempt{account_id, purpose};

  if (current_attempt_.has_value()) {
    // If we have two login attempts, let new attempt take
    // over the existing one.
    if (AttemptShouldOverrideAnother(attempt, *current_attempt_)) {
      LOG(WARNING) << "Overriding ongoing attempt";
      pending_attempt_ = attempt;
      pending_consumer_ = consumer;
      vector_lifecycle_->CancelAttempt();
      return;
    }
    if (AttemptShouldOverrideAnother(*current_attempt_, attempt)) {
      LOG(WARNING) << "Attempt rejected: another higher-priority attempt";
      consumer->OnUserAuthAttemptRejected();
      return;
    }
    // Neither attempt is considered "Stronger" one,
    // so we should preserve ongoing one.
    LOG(WARNING) << "Attempt rejected: another same-priority attempt";
    consumer->OnUserAuthAttemptRejected();
    return;
  }
  if (pending_attempt_.has_value()) {
    // If we have two login attempts, let new attempt take
    // over the pending one.
    if (AttemptShouldOverrideAnother(attempt, *pending_attempt_)) {
      LOG(WARNING) << "Overriding pending attempt";
      pending_consumer_->OnUserAuthAttemptRejected();
      // Override pending attempt.
      pending_attempt_ = attempt;
      pending_consumer_ = consumer;
      return;
    }
    if (AttemptShouldOverrideAnother(*pending_attempt_, attempt)) {
      LOG(WARNING)
          << "Attempt rejected: another higher-priority pending attempt";
      consumer->OnUserAuthAttemptRejected();
      return;
    }
    // Neither attempt is considered "Stronger" one,
    // so we should preserve pending one.
    LOG(WARNING) << "Attempt rejected: pending same-priority attempt";
    consumer->OnUserAuthAttemptRejected();
    return;
  }

  CHECK(!attempt_consumer_);
  attempt_consumer_ = consumer;
  current_attempt_ = attempt;
  vector_lifecycle_->StartAttempt(attempt);
}

bool AuthHubImpl::PurposeMatchesMode(AuthPurpose purpose, AuthHubMode mode) {
  switch (mode) {
    case kLoginScreen:
      return purpose == AuthPurpose::kLogin;
    case kInSession:
      return purpose != AuthPurpose::kLogin;
    case kNone:
      NOTREACHED_NORETURN();
  }
}

bool AuthHubImpl::AttemptShouldOverrideAnother(
    const AuthAttemptVector& first,
    const AuthAttemptVector& second) {
  if (first.purpose == AuthPurpose::kLogin &&
      second.purpose == AuthPurpose::kLogin) {
    // New login attempt always overrides previous.
    return true;
  }
  // All login cases should be covered by check above + `PurposeMatchesMode`.
  CHECK_NE(first.purpose, AuthPurpose::kLogin);
  CHECK_NE(second.purpose, AuthPurpose::kLogin);
  if (first.purpose == AuthPurpose::kScreenUnlock) {
    // Lock screen always overrides any other attempt.
    return true;
  }
  if (second.purpose == AuthPurpose::kScreenUnlock) {
    // Nothing in-session can override lock screen.
    return false;
  }
  // Currently various in-session attempts should not override ongoing attempt.
  return false;
}

// AuthHubModeLifecycle::Owner:

void AuthHubImpl::OnReadyForMode(AuthHubMode mode,
                                 AuthEnginesMap available_engines) {
  CHECK(engines_.empty());
  CHECK(!vector_lifecycle_);

  engines_ = std::move(available_engines);
  vector_lifecycle_ =
      std::make_unique<AuthHubVectorLifecycle>(this, mode, engines_);

  on_initialized_listeners_.Notify();
}

void AuthHubImpl::OnExitedMode(AuthHubMode mode) {}

void AuthHubImpl::OnModeShutdown() {}

// AuthHubVectorLifecycle::Owner:

AuthFactorEngine::FactorEngineObserver* AuthHubImpl::AsEngineObserver() {
  return this;
}

void AuthHubImpl::OnAttemptStarted(const AuthAttemptVector& attempt,
                                   AuthFactorsSet available_factors,
                                   AuthFactorsSet failed_factors) {
  base::raw_ptr<AuthFactorStatusConsumer> status_consumer;
  attempt_consumer_->OnUserAuthAttemptConfirmed(nullptr, status_consumer);
}

void AuthHubImpl::OnAttemptFinished(const AuthAttemptVector& attempt) {
  CHECK(attempt == *current_attempt_);
  attempt_consumer_ = nullptr;
  current_attempt_ = absl::nullopt;
}

void AuthHubImpl::OnIdle() {
  if (target_mode_.has_value()) {
    if (pending_attempt_.has_value()) {
      // Cancel pending attempt.
      pending_consumer_->OnUserAuthAttemptRejected();
      target_mode_ = absl::nullopt;
      pending_consumer_ = nullptr;
    }
    vector_lifecycle_.release();
    AuthHubMode mode = *target_mode_;
    target_mode_ = absl::nullopt;
    InitializeForMode(mode);
    return;
  }

  if (pending_attempt_.has_value()) {
    AuthAttemptVector attempt = *pending_attempt_;
    AuthAttemptConsumer* consumer = pending_consumer_.get();

    pending_consumer_ = nullptr;
    pending_attempt_ = absl::nullopt;

    StartAuthentication(attempt.account, attempt.purpose, consumer);
    return;
  }
}

// AuthFactorEngine::FactorEngineObserver:
void AuthHubImpl::OnFactorPresenceChecked(AshAuthFactor factor,
                                          bool factor_present) {
  // Ignored, checked by AuthHubVectorLifecycle, results are
  // passed to OnAttemptStarted.
}

void AuthHubImpl::OnFactorAttempt(AshAuthFactor factor) {}
void AuthHubImpl::OnFactorAttemptResult(AshAuthFactor factor, bool success) {}
void AuthHubImpl::OnPolicyChanged(AshAuthFactor factor) {}
void AuthHubImpl::OnLockoutChanged(AshAuthFactor factor) {}
void AuthHubImpl::OnOrientationRestrictionsChanged(AshAuthFactor factor) {}
void AuthHubImpl::OnCriticalError(AshAuthFactor factor) {}
void AuthHubImpl::OnFactorCustomSignal(AshAuthFactor factor) {}

}  // namespace ash
