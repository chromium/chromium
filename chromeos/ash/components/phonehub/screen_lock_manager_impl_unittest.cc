// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/screen_lock_manager_impl.h"

#include "chromeos/ash/components/phonehub/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

class FakeObserver : public ScreenLockManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // ScreenLockManager::Observer:
  void OnScreenLockChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class ScreenLockManagerImplTest : public testing::Test {
 protected:
  ScreenLockManagerImplTest() = default;
  ScreenLockManagerImplTest(const ScreenLockManagerImplTest&) = delete;
  ScreenLockManagerImplTest& operator=(const ScreenLockManagerImplTest&) =
      delete;
  ~ScreenLockManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    ScreenLockManagerImpl::RegisterPrefs(pref_service_.registry());
  }

  void TearDown() override { manager_->RemoveObserver(&fake_observer_); }

  void Initialize(ScreenLockManager::LockStatus expected_status) {
    pref_service_.SetInteger(prefs::kScreenLockStatus,
                             static_cast<int>(expected_status));
    manager_ = std::make_unique<ScreenLockManagerImpl>(&pref_service_);
    manager_->AddObserver(&fake_observer_);
  }

  void VerifyScreenLockState(ScreenLockManager::LockStatus expected_status) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kScreenLockStatus));
    EXPECT_EQ(expected_status, manager_->GetLockStatus());
  }

  void SetLockStatusInternal(ScreenLockManager::LockStatus status) {
    manager_->SetLockStatusInternal(status);
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  TestingPrefServiceSimple pref_service_;
  FakeObserver fake_observer_;
  std::unique_ptr<ScreenLockManager> manager_;
};

TEST_F(ScreenLockManagerImplTest, ScreenLockStateChanged) {
  Initialize(ScreenLockManager::LockStatus::kUnknown);
  VerifyScreenLockState(ScreenLockManager::LockStatus::kUnknown);

  // Simulate getting a response back from the phone.
  SetLockStatusInternal(ScreenLockManager::LockStatus::kLockedOn);
  VerifyScreenLockState(ScreenLockManager::LockStatus::kLockedOn);
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetLockStatusInternal(ScreenLockManager::LockStatus::kLockedOff);
  VerifyScreenLockState(ScreenLockManager::LockStatus::kLockedOff);
  EXPECT_EQ(2u, GetNumObserverCalls());
}

}  // namespace phonehub
}  // namespace ash
