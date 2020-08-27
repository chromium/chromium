// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/find_my_device_controller_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public FindMyDeviceController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // FindMyDeviceController::Observer:
  void OnPhoneRingingStateChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class FindMyDeviceControllerImplTest : public testing::Test {
 protected:
  FindMyDeviceControllerImplTest() = default;
  FindMyDeviceControllerImplTest(const FindMyDeviceControllerImplTest&) =
      delete;
  FindMyDeviceControllerImplTest& operator=(
      const FindMyDeviceControllerImplTest&) = delete;
  ~FindMyDeviceControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    controller_ = std::make_unique<FindMyDeviceControllerImpl>();
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  bool IsPhoneRinging() const { return controller_->IsPhoneRinging(); }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<FindMyDeviceController> controller_;
};

// TODO(https://crbug.com/1106937): Remove this test once we have real
// functionality to test.
TEST_F(FindMyDeviceControllerImplTest, Initialize) {
  EXPECT_FALSE(IsPhoneRinging());
}

}  // namespace phonehub
}  // namespace chromeos
