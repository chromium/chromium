// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/fake_bulk_leak_check_service.h"

#import "base/task/sequenced_task_runner.h"

namespace password_manager {

constexpr base::TimeDelta kDelay = base::Seconds(1);

FakeBulkLeakCheckService::FakeBulkLeakCheckService() = default;
FakeBulkLeakCheckService::~FakeBulkLeakCheckService() = default;

#pragma mark - BulkLeakCheckServiceInterface

void FakeBulkLeakCheckService::CheckUsernamePasswordPairs(
    LeakDetectionInitiator initiator,
    std::vector<LeakCheckCredential> credentials) {
  state_ = State::kRunning;
  NotifyStateChanged();

  // Schedule SetStateToBufferedState to be called with a delay to imitate the
  // fact that the BulkLeakCheckService normally ends after the weak and reused
  // password checks.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBulkLeakCheckService::SetStateToBufferedState,
                     weak_ptr_factory_.GetWeakPtr()),
      kDelay);
}

void FakeBulkLeakCheckService::Cancel() {}

size_t FakeBulkLeakCheckService::GetPendingChecksCount() const {
  return 0;
}

BulkLeakCheckServiceInterface::State FakeBulkLeakCheckService::GetState()
    const {
  return state_;
}

void FakeBulkLeakCheckService::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void FakeBulkLeakCheckService::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

#pragma mark - Setters

void FakeBulkLeakCheckService::SetBufferedState(
    BulkLeakCheckServiceInterface::State state) {
  buffered_state_ = state;
}

#pragma mark - Private

void FakeBulkLeakCheckService::NotifyStateChanged() {
  for (Observer& obs : observers_) {
    obs.OnStateChanged(state_);
  }
}

void FakeBulkLeakCheckService::SetStateToBufferedState() {
  state_ = buffered_state_;
  NotifyStateChanged();
}

}  // namespace password_manager
