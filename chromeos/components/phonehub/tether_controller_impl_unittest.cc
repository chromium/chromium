// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/tether_controller_impl.h"

#include <memory>

#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public TetherController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // TetherController::Observer:
  void OnTetherStatusChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class TetherControllerImplTest : public testing::Test {
 protected:
  TetherControllerImplTest() = default;
  TetherControllerImplTest(const TetherControllerImplTest&) = delete;
  TetherControllerImplTest& operator=(const TetherControllerImplTest&) = delete;
  ~TetherControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    controller_ =
        std::make_unique<TetherControllerImpl>(&fake_multidevice_setup_client_);
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  TetherController::Status GetStatus() const {
    return controller_->GetStatus();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;

  FakeObserver fake_observer_;
  std::unique_ptr<TetherController> controller_;
};

// TODO(khorimoto): Remove this test once we have real functionality to test.
TEST_F(TetherControllerImplTest, Initialize) {
  EXPECT_EQ(TetherController::Status::kIneligibleForFeature, GetStatus());
}

}  // namespace phonehub
}  // namespace chromeos
