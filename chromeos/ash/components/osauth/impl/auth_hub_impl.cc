// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/osauth/impl/auth_factor_presence_cache.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_vector_lifecycle.h"
#include "chromeos/ash/components/osauth/public/auth_attempt_consumer.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/string_utils.h"
#include "components/account_id/account_id.h"

namespace ash {

AuthHubImpl::AuthHubImpl(AuthFactorPresenceCache* factor_cache)
    : cache_(factor_cache) {
  mode_lifecycle_ = std::make_unique<AuthHubModeLifecycle>(this);
}

AuthHubImpl::~AuthHubImpl() = default;

void AuthHubImpl::InitializeForMode(AuthHubMode target) {
  CHECK_NE(target, AuthHubMode::kNone);
  SwitchToModeImpl(target);
}

void AuthHubImpl::CancelCurrentAttempt(AuthHubConnector* connector) {
  if (attempt_handler_->HasOngoingAttempt()) {
    attempt_handler_->PrepareForShutdown(
        base::BindOnce(&AuthHubImpl::OnFactorAttemptFinishedForCancel,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  vector_lifecycle_->CancelAttempt();
}

void AuthHubImpl::Shutdown() {
  SwitchToModeImpl(AuthHubMode::kNone);
}

void AuthHubImpl::SwitchToModeImpl(AuthHubMode target) {
  if (vector_lifecycle_ && !vector_lifecycle_->IsIdle()) {
    target_mode_ = target;
    // Eventually, after the current attempt gets canceled, `OnIdle()` will be
    // triggered, which then switches the mode to `target_mode_`.
    if (attempt_handler_->HasOngoingAttempt()) {
      attempt_handler_->PrepareForShutdown(
          base::BindOnce(&AuthHubImpl::OnFactorAttemptFinishedForCancel,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    vector_lifecycle_->CancelAttempt();
    return;
  }
  mode_lifecycle_->SwitchToMode(target);
}

void AuthHubImpl::OnFactorAttemptFinishedForCancel() {
  vector_lifecycle_->CancelAttempt();
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
      if (attempt_handler_->HasOngoingAttempt()) {
        attempt_handler_->PrepareForShutdown(
            base::BindOnce(&AuthHubImpl::OnFactorAttemptFinishedForCancel,
                           weak_factory_.GetWeakPtr()));
        return;
      }
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
  CHECK(!attempt_handler_);
  attempt_consumer_ = consumer;
  current_attempt_ = attempt;

  AuthFactorsSet cached_factors =
      cache_->GetExpectedFactorsPresence(*current_attempt_);

  attempt_handler_ = std::make_unique<AuthHubAttemptHandler>(
      this, *current_attempt_, engines_, cached_factors);
  raw_ptr<AuthFactorStatusConsumer> status_consumer;
  attempt_consumer_->OnUserAuthAttemptConfirmed(
      attempt_handler_->GetConnector(), status_consumer);
  attempt_handler_->SetConsumer(status_consumer);

  vector_lifecycle_->StartAttempt(*current_attempt_);
}

bool AuthHubImpl::PurposeMatchesMode(AuthPurpose purpose, AuthHubMode mode) {
  switch (mode) {
    case AuthHubMode::kLoginScreen:
      return purpose == AuthPurpose::kLogin;
    case AuthHubMode::kInSession:
      return purpose != AuthPurpose::kLogin;
    case AuthHubMode::kNone:
      NOTREACHED();
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

void AuthHubImpl::OnExitedMode(AuthHubMode mode) {
  engines_.clear();
  vector_lifecycle_.reset();
}

void AuthHubImpl::OnModeShutdown() {}

// AuthHubVectorLifecycle::Owner:

AuthFactorEngine::FactorEngineObserver* AuthHubImpl::AsEngineObserver() {
  CHECK(attempt_handler_);
  return attempt_handler_.get();
}

void AuthHubImpl::OnAttemptStarted(const AuthAttemptVector& attempt,
                                   AuthFactorsSet available_factors,
                                   AuthFactorsSet failed_factors) {
  CHECK(attempt == *current_attempt_);
  CHECK(attempt_handler_);
  attempt_handler_->OnFactorsChecked(available_factors, failed_factors);
}

void AuthHubImpl::OnAttemptCleanedUp(const AuthAttemptVector& attempt) {
  CHECK(attempt == *current_attempt_);
  if (authenticated_factor_.has_value()) {
    AuthProofToken token =
        engines_[*authenticated_factor_]->StoreAuthenticationContext();
    attempt_consumer_->OnUserAuthSuccess(*authenticated_factor_, token);
    authenticated_factor_.reset();
  }
}

void AuthHubImpl::OnAttemptFinished(const AuthAttemptVector& attempt) {
  CHECK(attempt == *current_attempt_);
  attempt_consumer_ = nullptr;
  current_attempt_.reset();
  attempt_handler_.reset();
}

void AuthHubImpl::OnAttemptCancelled(const AuthAttemptVector& attempt) {
  CHECK(attempt == *current_attempt_);
  attempt_consumer_->OnUserAuthAttemptCancelled();
  attempt_consumer_ = nullptr;
  current_attempt_.reset();
  attempt_handler_.reset();
}

void AuthHubImpl::OnIdle() {
  if (target_mode_.has_value()) {
    if (pending_attempt_.has_value()) {
      // Cancel pending attempt.
      pending_consumer_->OnUserAuthAttemptRejected();
      target_mode_.reset();
      pending_consumer_ = nullptr;
    }
    // We can get into this branch if attempt was never
    // started/finished, e.g. if Mode change was requested before
    // one of the engines started.
    if (attempt_consumer_) {
      CHECK(current_attempt_);
      CHECK(attempt_handler_);
      attempt_consumer_->OnUserAuthAttemptCancelled();
      attempt_consumer_ = nullptr;
      current_attempt_.reset();
      attempt_handler_.reset();
    }
    AuthHubMode mode = *target_mode_;
    target_mode_.reset();
    SwitchToModeImpl(mode);
    return;
  }

  if (pending_attempt_.has_value()) {
    AuthAttemptVector attempt = *pending_attempt_;
    AuthAttemptConsumer* consumer = pending_consumer_.get();

    pending_consumer_ = nullptr;
    pending_attempt_.reset();

    StartAuthentication(attempt.account, attempt.purpose, consumer);
    return;
  }
}

void AuthHubImpl::UpdateFactorUiCache(const AuthAttemptVector& attempt,
                                      AuthFactorsSet available_factors) {
  CHECK(attempt == *current_attempt_);
  cache_->StoreFactorPresenceCache(attempt, available_factors);
}

void AuthHubImpl::OnFactorAttemptFailed(const AuthAttemptVector& attempt,
                                        AshAuthFactor factor) {
  CHECK(attempt == *current_attempt_);
  attempt_consumer_->OnFactorAttemptFailed(factor);
}

void AuthHubImpl::OnAuthenticationSuccess(const AuthAttemptVector& attempt,
                                          AshAuthFactor factor) {
  CHECK(attempt == *current_attempt_);
  CHECK(engines_.contains(factor));

  // Record the authenticated factor and start terminating all engines.
  // AuthProofToken will be retrieved after all auth engines clean up their
  // internal states during the termination process, to avoid race conditions
  // on authenticated UserContext.
  authenticated_factor_ = factor;
  vector_lifecycle_->CancelAttempt();
}

}  // namespace ash
