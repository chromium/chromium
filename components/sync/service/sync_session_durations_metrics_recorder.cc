// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_session_durations_metrics_recorder.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

namespace syncer {

namespace {

base::TimeDelta SubtractInactiveTime(base::TimeDelta total_length,
                                     base::TimeDelta inactive_time) {
  // Subtract any time the user was inactive from our session length. If this
  // ends up giving the session negative length, which can happen if the feature
  // state changed after the user became inactive, log the length as 0.
  base::TimeDelta session_length = total_length - inactive_time;
  if (session_length.is_negative()) {
    session_length = base::TimeDelta();
  }
  return session_length;
}

void LogDuration(const std::string& histogram_suffix,
                 base::TimeDelta session_length) {
  DVLOG(1) << "Logging Session.TotalDuration*." + histogram_suffix << " of "
           << session_length;
  // Log the 1-day long session duration histograms.
  base::UmaHistogramCustomTimes(
      "Session.TotalDurationMax1Day." + histogram_suffix, session_length,
      base::Milliseconds(1), base::Hours(24), 50);

  // Log the legacy 1-hour long histogram.
  // TODO(crbug.com/40859574): Remove these histograms once they are no
  // longer used to generate the sign-in and sync usage dashboards.
  base::UmaHistogramLongTimes("Session.TotalDuration." + histogram_suffix,
                              session_length);
}

}  // namespace

SyncSessionDurationsMetricsRecorder::SyncSessionDurationsMetricsRecorder(
    SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : sync_service_(sync_service),
      identity_manager_(identity_manager),
      history_sync_recorder_(sync_service) {
  // |sync_service| can be null if sync is disabled by a command line flag.
  if (sync_service_) {
    sync_observation_.Observe(sync_service_.get());
  }
  identity_manager_observation_.Observe(identity_manager_.get());

  // Since this is created after the profile itself is created, we need to
  // handle the initial state.
  signin_status_ = DetermineSigninStatus();
  sync_status_ = DetermineSyncStatus();

  // Check if we already know the signed in cookies. This will trigger a fetch
  // if we don't have them yet.
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar_info.AreAccountsFresh()) {
    OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                              GoogleServiceAuthError::AuthErrorNone());
  }

  DVLOG(1) << "Ready to track Session.TotalDuration metrics";
}

SyncSessionDurationsMetricsRecorder::~SyncSessionDurationsMetricsRecorder() {
  DCHECK(!total_session_timer_) << "Missing a call to OnSessionEnded().";
  sync_observation_.Reset();
  DCHECK(identity_manager_observation_.IsObserving());
  identity_manager_observation_.Reset();
}

SyncSessionDurationsMetricsRecorder::SigninStatus
SyncSessionDurationsMetricsRecorder::GetSigninStatus() const {
  return signin_status_;
}

bool SyncSessionDurationsMetricsRecorder::IsSyncing() const {
  return signin_status_ == SigninStatus::kSignedIn &&
         sync_status_ == FeatureState::ON;
}

void SyncSessionDurationsMetricsRecorder::OnSessionStarted(
    base::TimeTicks session_start) {
  DVLOG(1) << "Session start";
  total_session_timer_ = std::make_unique<base::ElapsedTimer>();
  signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
  sync_account_session_timer_ = std::make_unique<base::ElapsedTimer>();

  history_sync_recorder_.OnSessionStarted(session_start);
}

void SyncSessionDurationsMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length) {
  DVLOG(1) << "Session end";

  history_sync_recorder_.OnSessionEnded(session_length);

  if (!total_session_timer_) {
    // If there was no active session, just ignore this call.
    return;
  }

  if (session_length.is_zero()) {
    // During Profile teardown, this method is called with a |session_length|
    // of zero.
    session_length = total_session_timer_->Elapsed();
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
           << accounts_in_cookie_jar_info
                  .GetPotentiallyInvalidSignedInAccounts()
                  .size()
           << " fresh: " << accounts_in_cookie_jar_info.AreAccountsFresh()
           << " err: " << error.ToString();

  if (error.state() != GoogleServiceAuthError::NONE) {
    // Return early if there's an error. This should only happen if there's an
    // actual error getting the account list. If there are any auth errors with
    // the tokens, those accounts will be moved to signed out accounts in
    // AccountsInCookieJarInfo.
    return;
  }

  DCHECK(accounts_in_cookie_jar_info.AreAccountsFresh());
  if (accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
          .empty()) {
    // No signed in account.
    if (cookie_signin_status_ == FeatureState::ON && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    cookie_signin_status_ = FeatureState::OFF;
  } else {
    // There is a signed in account.
    if (cookie_signin_status_ == FeatureState::OFF && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    cookie_signin_status_ = FeatureState::ON;
  }
}

void SyncSessionDurationsMetricsRecorder::OnStateChanged(SyncService* sync) {
  DVLOG(1) << "Sync state change";
  HandleSyncAndAccountChange();
}

void SyncSessionDurationsMetricsRecorder::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  DVLOG(1) << __func__;
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
        const GoogleServiceAuthError& error,
        signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  DVLOG(1) << __func__;
  HandleSyncAndAccountChange();
}

bool SyncSessionDurationsMetricsRecorder::ShouldLogUpdate(
    FeatureState new_sync_status,
    SigninStatus new_signin_status) {
  bool sync_status_changed = new_sync_status != sync_status_;
  bool signin_status_changed = new_signin_status != signin_status_;
  bool status_changed = sync_status_changed || signin_status_changed;
  bool was_unknown = sync_status_ == FeatureState::UNKNOWN;
  return sync_account_session_timer_ && status_changed && !was_unknown;
}

void SyncSessionDurationsMetricsRecorder::UpdateSyncAndAccountStatus(
    FeatureState new_sync_status,
    SigninStatus new_signin_status) {
  DVLOG(1) << "UpdateSyncAndAccountStatus:" << " new_sync_status: "
           << static_cast<int>(new_sync_status)
           << " new_signin_status: " << static_cast<int>(new_signin_status);

  // |new_sync_status| may be unknown when there is a primary account, but
  // the sync engine has not yet started.
  if (ShouldLogUpdate(new_sync_status, new_signin_status)) {
    LogSyncAndAccountDuration(sync_account_session_timer_->Elapsed());
    sync_account_session_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  sync_status_ = new_sync_status;
  signin_status_ = new_signin_status;
}

void SyncSessionDurationsMetricsRecorder::HandleSyncAndAccountChange() {
  UpdateSyncAndAccountStatus(DetermineSyncStatus(), DetermineSigninStatus());
}

// static
constexpr int SyncSessionDurationsMetricsRecorder::GetFeatureStates(
    SigninStatus signin_status,
    FeatureState sync_status) {
  return 100 * static_cast<int>(signin_status) + static_cast<int>(sync_status);
}

void SyncSessionDurationsMetricsRecorder::LogSigninDuration(
    base::TimeDelta session_length) {
  switch (cookie_signin_status_) {
    case FeatureState::ON:
      LogDuration("WithAccount", session_length);
      break;
    case FeatureState::UNKNOWN:
      // Since the feature wasn't working for the user if we didn't know its
      // state, log the status as off.
      [[fallthrough]];
    case FeatureState::OFF:
      LogDuration("WithoutAccount", session_length);
      break;
  }
}

void SyncSessionDurationsMetricsRecorder::LogSyncAndAccountDuration(
    base::TimeDelta session_length) {
  switch (GetFeatureStates(signin_status_, sync_status_)) {
    case GetFeatureStates(SigninStatus::kSignedIn, FeatureState::ON):
      LogDuration("OptedInToSyncWithAccount", session_length);
      break;
    case GetFeatureStates(SigninStatus::kSignedIn, FeatureState::UNKNOWN):
      // Sync engine not initialized yet, default to it being off.
      [[fallthrough]];
    case GetFeatureStates(SigninStatus::kSignedIn, FeatureState::OFF):
      LogDuration("NotOptedInToSyncWithAccount", session_length);
      break;
    case GetFeatureStates(SigninStatus::kSignedInWithError, FeatureState::ON):
      LogDuration("OptedInToSyncWithoutAccount", session_length);
      break;
    case GetFeatureStates(SigninStatus::kSignedInWithError,
                          FeatureState::UNKNOWN):
      // Sync engine not initialized yet, default to it being off.
    case GetFeatureStates(SigninStatus::kSignedInWithError, FeatureState::OFF):
      LogDuration("NotOptedInToSyncWithAccountInAuthError", session_length);
      break;
    case GetFeatureStates(SigninStatus::kSignedOut, FeatureState::UNKNOWN):
      // Sync engine not initialized yet, default to it being off.
    case GetFeatureStates(SigninStatus::kSignedOut, FeatureState::OFF):
      LogDuration("NotOptedInToSyncWithoutAccount", session_length);
      break;
    case GetFeatureStates(SigninStatus::kSignedOut, FeatureState::ON):
      // This state cannot happen in production, but does happen in tests.
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected feature states: "
          << GetFeatureStates(signin_status_, sync_status_);
      break;
  }
}

SyncSessionDurationsMetricsRecorder::SigninStatus
SyncSessionDurationsMetricsRecorder::DetermineSigninStatus() const {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return SigninStatus::kSignedOut;
  }

  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  return (identity_manager_->HasAccountWithRefreshToken(primary_account_id) &&
          !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
              primary_account_id))
             ? SigninStatus::kSignedIn
             : SigninStatus::kSignedInWithError;
}

SyncSessionDurationsMetricsRecorder::FeatureState
SyncSessionDurationsMetricsRecorder::DetermineSyncStatus() const {
  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  if (!sync_service_ || !sync_service_->CanSyncFeatureStart()) {
    return FeatureState::OFF;
  }

  if (sync_service_->GetTransportState() ==
      SyncService::TransportState::PAUSED) {
    // Sync is considered to be ON even when paused.
    return FeatureState::ON;
  }

  if (sync_service_->IsSyncFeatureActive() &&
      sync_service_->HasCompletedSyncCycle()) {
    return FeatureState::ON;
  }

  // This branch corresponds to the case when the sync engine is initializing.
  //
  // The sync state may already be set to ON/OFF if updated previously. Return
  // the current sync status.
  //
  // Note: It is possible for |sync_status_| to be ON/OFF at this point. This
  // corresponds to sync state transitions that can happen if a turns sync on
  // or off. For example if during browser startup there is no signed-in user,
  /// then |sync_state_| is OFF. When the user turns on Sync, the sync state
  // is essentially unknown for a while - the current implementation keeps
  // previous |sync_state_|.
  return sync_status_;
}

}  // namespace syncer
