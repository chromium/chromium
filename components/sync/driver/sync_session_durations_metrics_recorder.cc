// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_session_durations_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

namespace syncer {

namespace {
base::TimeDelta SubtractInactiveTime(base::TimeDelta total_length,
                                     base::TimeDelta inactive_time) {
  // Substract any time the user was inactive from our session length. If this
  // ends up giving the session negative length, which can happen if the feature
  // state changed after the user became inactive, log the length as 0.
  base::TimeDelta session_length = total_length - inactive_time;
  if (session_length < base::TimeDelta()) {
    session_length = base::TimeDelta();
  }
  return session_length;
}
}  // namespace

SyncSessionDurationsMetricsRecorder::SyncSessionDurationsMetricsRecorder(
    SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : sync_service_(sync_service), identity_manager_(identity_manager) {
  // |sync_service| can be null if sync is disabled by a command line flag.
  if (sync_service_) {
    sync_observer_.Add(sync_service_);
  }
  identity_manager_observer_.Add(identity_manager_);

  // Since this is created after the profile itself is created, we need to
  // handle the initial state.
  HandleSyncAndAccountChange();

  // Check if we already know the signed in cookies. This will trigger a fetch
  // if we don't have them yet.
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar_info.accounts_are_fresh) {
    OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                              GoogleServiceAuthError::AuthErrorNone());
  }

  DVLOG(1) << "Ready to track Session.TotalDuration metrics";
}

SyncSessionDurationsMetricsRecorder::~SyncSessionDurationsMetricsRecorder() {
  DCHECK(!total_session_timer_) << "Missing a call to OnSessionEnded().";
  sync_observer_.RemoveAll();
  identity_manager_observer_.RemoveAll();
}

void SyncSessionDurationsMetricsRecorder::OnSessionStarted(
    base::TimeTicks session_start) {
  DVLOG(1) << "Session start";
  total_session_timer_ = std::make_unique<base::ElapsedTimer>();
  signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
  sync_account_session_timer_ = std::make_unique<base::ElapsedTimer>();
}

void SyncSessionDurationsMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length) {
  DVLOG(1) << "Session end";

  if (!total_session_timer_) {
    // If there was no active session, just ignore this call.
    return;
  }

  base::TimeDelta total_session_time = total_session_timer_->Elapsed();
  base::TimeDelta signin_session_time = signin_session_timer_->Elapsed();
  base::TimeDelta sync_account_session_time_ =
      sync_account_session_timer_->Elapsed();
  total_session_timer_.reset();
  signin_session_timer_.reset();
  sync_account_session_timer_.reset();

  base::TimeDelta total_inactivity_time = total_session_time - session_length;
  LogSigninDuration(
      SubtractInactiveTime(signin_session_time, total_inactivity_time));
  LogSyncAndAccountDuration(
      SubtractInactiveTime(sync_account_session_time_, total_inactivity_time));
}

void SyncSessionDurationsMetricsRecorder::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Cookie state change. accounts: "
           << accounts_in_cookie_jar_info.signed_in_accounts.size()
           << " fresh: " << accounts_in_cookie_jar_info.accounts_are_fresh
           << " err: " << error.ToString();

  if (error.state() != GoogleServiceAuthError::NONE) {
    // Return early if there's an error. This should only happen if there's an
    // actual error getting the account list. If there are any auth errors with
    // the tokens, those accounts will be moved to signed_out_accounts instead.
    return;
  }

  DCHECK(accounts_in_cookie_jar_info.accounts_are_fresh);
  if (accounts_in_cookie_jar_info.signed_in_accounts.empty()) {
    // No signed in account.
    if (signin_status_ == FeatureState::ON && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    signin_status_ = FeatureState::OFF;
  } else {
    // There is a signed in account.
    if (signin_status_ == FeatureState::OFF && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    signin_status_ = FeatureState::ON;
  }
}

void SyncSessionDurationsMetricsRecorder::OnStateChanged(SyncService* sync) {
  DVLOG(1) << "Sync state change";
  HandleSyncAndAccountChange();
}

void SyncSessionDurationsMetricsRecorder::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

void SyncSessionDurationsMetricsRecorder::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

void SyncSessionDurationsMetricsRecorder::OnRefreshTokensLoaded() {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

void SyncSessionDurationsMetricsRecorder::
    OnErrorStateOfRefreshTokenUpdatedForAccount(
        const CoreAccountInfo& account_info,
        const GoogleServiceAuthError& error) {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

bool SyncSessionDurationsMetricsRecorder::ShouldLogUpdate(
    FeatureState new_sync_status,
    FeatureState new_account_status) {
  bool status_change = (new_sync_status != sync_status_ ||
                        new_account_status != account_status_);
  bool was_unknown = sync_status_ == FeatureState::UNKNOWN ||
                     account_status_ == FeatureState::UNKNOWN;
  return sync_account_session_timer_ && status_change && !was_unknown;
}

void SyncSessionDurationsMetricsRecorder::UpdateSyncAndAccountStatus(
    FeatureState new_sync_status,
    FeatureState new_account_status) {
  if (ShouldLogUpdate(new_sync_status, new_account_status)) {
    LogSyncAndAccountDuration(sync_account_session_timer_->Elapsed());
    sync_account_session_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  sync_status_ = new_sync_status;
  account_status_ = new_account_status;
}

void SyncSessionDurationsMetricsRecorder::HandleSyncAndAccountChange() {
  if (!sync_service_ || !sync_service_->CanSyncFeatureStart()) {
    // Only the account status needs to be updated when sync is off.
    UpdateSyncAndAccountStatus(FeatureState::OFF, DetermineAccountStatus());
    return;
  }

  // Sync has potential to turn on, or get into account error state.
  if (sync_service_->GetAuthError().state() ==
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    // Sync is enabled, but we have an account issue.
    UpdateSyncAndAccountStatus(FeatureState::ON, FeatureState::OFF);
  } else if (sync_service_->IsSyncFeatureActive() &&
             sync_service_->HasCompletedSyncCycle()) {
    // Sync is on and running, we must have an account too.
    UpdateSyncAndAccountStatus(FeatureState::ON, FeatureState::ON);
  } else {
    // We don't know yet if sync is going to work.
    // At least update the account status, so that if we never learn what the
    // sync state is, we know the signin state.
    //
    // TODO(msarda): The current code uses the account status for all accounts
    // (i.e. it is not scoped to the sync account). Figure out whether this
    // should be changed to only capture the status of the sync account when
    // the user has opted in to sync.
    account_status_ = DetermineAccountStatus();
  }
}

void SyncSessionDurationsMetricsRecorder::LogSigninDuration(
    base::TimeDelta session_length) {
  switch (signin_status_) {
    case FeatureState::ON:
      DVLOG(1) << "Logging Session.TotalDuration.WithAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.WithAccount",
                               session_length);
      break;
    case FeatureState::OFF:
    // Since the feature wasn't working for the user if we didn't know its
    // state, log the status as off.
    case FeatureState::UNKNOWN:
      DVLOG(1) << "Logging Session.TotalDuration.WithoutAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.WithoutAccount",
                               session_length);
  }
}

void SyncSessionDurationsMetricsRecorder::LogSyncAndAccountDuration(
    base::TimeDelta session_length) {
  if (sync_status_ == FeatureState::ON) {
    if (account_status_ == FeatureState::ON) {
      DVLOG(1) << "Logging Session.TotalDuration.OptedInToSyncWithAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.OptedInToSyncWithAccount",
                               session_length);
    } else {
      DVLOG(1)
          << "Logging Session.TotalDuration.OptedInToSyncWithoutAccount of "
          << session_length;
      UMA_HISTOGRAM_LONG_TIMES(
          "Session.TotalDuration.OptedInToSyncWithoutAccount", session_length);
    }
  } else {
    if (account_status_ == FeatureState::ON) {
      DVLOG(1)
          << "Logging Session.TotalDuration.NotOptedInToSyncWithAccount of "
          << session_length;
      UMA_HISTOGRAM_LONG_TIMES(
          "Session.TotalDuration.NotOptedInToSyncWithAccount", session_length);
    } else {
      DVLOG(1)
          << "Logging Session.TotalDuration.NotOptedInToSyncWithoutAccount of "
          << session_length;
      UMA_HISTOGRAM_LONG_TIMES(
          "Session.TotalDuration.NotOptedInToSyncWithoutAccount",
          session_length);
    }
  }
}

SyncSessionDurationsMetricsRecorder::FeatureState
SyncSessionDurationsMetricsRecorder::DetermineAccountStatus() const {
  for (const auto& account :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account.account_id)) {
      return SyncSessionDurationsMetricsRecorder::FeatureState::ON;
    }
  }
  return SyncSessionDurationsMetricsRecorder::FeatureState::OFF;
}

}  // namespace syncer
