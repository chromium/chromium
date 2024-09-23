// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

// Helper class that collects UMA about the service.
class BulkLeakCheckService::MetricsReporter {
 public:
  MetricsReporter() = default;
  ~MetricsReporter();
  MetricsReporter(const MetricsReporter&) = delete;
  MetricsReporter& operator=(const MetricsReporter&) = delete;

  void OnStartCheck(size_t credential_count);
  void OnCredentialChecked(IsLeaked is_leaked);
  void OnCancelCheck();
  void OnError(LeakDetectionError error);

 private:
  base::ElapsedTimer timer_since_start_;
  size_t credential_count_ = 0;
  size_t leaked_credential_count_ = 0;
  bool error_or_canceled_ = false;
};

BulkLeakCheckService::MetricsReporter::~MetricsReporter() {
  if (!credential_count_ || error_or_canceled_) {
    return;
  }

  base::UmaHistogramMediumTimes("PasswordManager.BulkCheck.Time",
                                timer_since_start_.Elapsed());
  base::UmaHistogramCounts1000("PasswordManager.BulkCheck.CheckedCredentials",
                               credential_count_);
  base::UmaHistogramCounts100("PasswordManager.BulkCheck.LeaksFound",
                              leaked_credential_count_);
}

void BulkLeakCheckService::MetricsReporter::OnStartCheck(
    size_t credential_count) {
  credential_count_ += credential_count;
}

void BulkLeakCheckService::MetricsReporter::OnCredentialChecked(
    IsLeaked is_leaked) {
  if (is_leaked) {
    leaked_credential_count_++;
  }
}

void BulkLeakCheckService::MetricsReporter::OnCancelCheck() {
  error_or_canceled_ = true;
}

void BulkLeakCheckService::MetricsReporter::OnError(LeakDetectionError error) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.BulkCheck.Error", error);

  error_or_canceled_ = true;
}

BulkLeakCheckService::BulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      leak_check_factory_(std::make_unique<LeakDetectionCheckFactoryImpl>()) {}

BulkLeakCheckService::~BulkLeakCheckService() = default;

void BulkLeakCheckService::CheckUsernamePasswordPairs(
    LeakDetectionInitiator initiator,
    std::vector<password_manager::LeakCheckCredential> credentials) {
  DVLOG(0) << "Bulk password check, start " << credentials.size();
  if (credentials.empty()) {
    // Nothing to check. Still important to go through the running state, so the
    // observers know that the results are available.
    state_ = State::kRunning;
    NotifyStateChanged();
    state_ = State::kIdle;
    NotifyStateChanged();
    return;
  }
  if (!metrics_reporter_) {
    metrics_reporter_ = std::make_unique<MetricsReporter>();
  }
  metrics_reporter_->OnStartCheck(credentials.size());
  if (bulk_leak_check_) {
    DCHECK_EQ(State::kRunning, state_);
    // The check is already running. Append the credentials to the list.
    bulk_leak_check_->CheckCredentials(initiator, std::move(credentials));
    // Notify the observers because the number of pending credentials changed.
    NotifyStateChanged();
    return;
  }

  bulk_leak_check_ = leak_check_factory_->TryCreateBulkLeakCheck(
      this, identity_manager_, url_loader_factory_);
  if (!bulk_leak_check_) {
    // The factory may have called OnError() so the service contains the correct
    // error state.
    return;
  }
  // The state is 'running now'. CheckCredentials() can trigger OnError() that
  // will change it to something else.
  state_ = State::kRunning;
  bulk_leak_check_->CheckCredentials(initiator, std::move(credentials));
  // Notify the observers after the call because the number of pending
  // credentials after CheckCredentials.
  NotifyStateChanged();
}

void BulkLeakCheckService::Cancel() {
  DVLOG(0) << "Bulk password check cancel";
  if (metrics_reporter_) {
    metrics_reporter_->OnCancelCheck();
    metrics_reporter_.reset();
  }
  if (!bulk_leak_check_) {
    DCHECK_NE(State::kRunning, state_);
    return;
  }
  state_ = State::kCanceled;
  bulk_leak_check_.reset();
  NotifyStateChanged();
}

size_t BulkLeakCheckService::GetPendingChecksCount() const {
  return bulk_leak_check_ ? bulk_leak_check_->GetPendingChecksCount() : 0;
}

BulkLeakCheckService::State BulkLeakCheckService::GetState() const {
  return state_;
}

void BulkLeakCheckService::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void BulkLeakCheckService::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void BulkLeakCheckService::Shutdown() {
  for (Observer& obs : observers_) {
    obs.OnBulkCheckServiceShutDown();
  }
  observers_.Clear();
  metrics_reporter_.reset();
  bulk_leak_check_.reset();
  url_loader_factory_.reset();
  identity_manager_ = nullptr;
}

void BulkLeakCheckService::OnFinishedCredential(LeakCheckCredential credential,
                                                IsLeaked is_leaked) {
  // (1) Make sure that the state of the service is correct.
  // (2) Notify about the leak if necessary.
  // (3) Notify about new state. The clients may assume that if the state is
  // idle then there won't be calls to OnLeakFound.
  metrics_reporter_->OnCredentialChecked(is_leaked);
  if (!GetPendingChecksCount()) {
    DVLOG(0) << "Bulk password check finished";
    state_ = State::kIdle;
    metrics_reporter_.reset();
    bulk_leak_check_.reset();
  }
  for (Observer& obs : observers_) {
    obs.OnCredentialDone(credential, is_leaked);
  }
  if (state_ == State::kIdle) {
    NotifyStateChanged();
  }
}

void BulkLeakCheckService::OnError(LeakDetectionError error) {
  DLOG(ERROR) << "Bulk password check error=" << static_cast<int>(error);
  metrics_reporter_->OnError(error);
  metrics_reporter_.reset();
  switch (error) {
    case LeakDetectionError::kNotSignIn:
      state_ = State::kSignedOut;
      break;
    case LeakDetectionError::kTokenRequestFailure:
      state_ = State::kTokenRequestFailure;
      break;
    case LeakDetectionError::kHashingFailure:
      state_ = State::kHashingFailure;
      break;
    case LeakDetectionError::kInvalidServerResponse:
      state_ = State::kServiceError;
      break;
    case LeakDetectionError::kNetworkError:
      state_ = State::kNetworkError;
      break;
    case LeakDetectionError::kQuotaLimit:
      state_ = State::kQuotaLimit;
      break;
  }
  bulk_leak_check_.reset();
  NotifyStateChanged();
}

void BulkLeakCheckService::NotifyStateChanged() {
  for (Observer& obs : observers_) {
    obs.OnStateChanged(state_);
  }
}

}  // namespace password_manager
