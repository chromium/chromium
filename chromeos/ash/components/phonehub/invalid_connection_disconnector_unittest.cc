// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/invalid_connection_disconnector.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

class InvalidConnectionDisconnectorTest : public testing::Test {
 public:
  InvalidConnectionDisconnectorTest() = default;
  InvalidConnectionDisconnectorTest(const InvalidConnectionDisconnectorTest&) =
      delete;
  InvalidConnectionDisconnectorTest& operator=(
      const InvalidConnectionDisconnectorTest&) = delete;
  ~InvalidConnectionDisconnectorTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();

    auto timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();

    invalid_connection_disconnector_ =
        base::WrapUnique(new InvalidConnectionDisconnector(
            fake_connection_manager_.get(), &fake_phone_model_,
            std::move(timer)));
  }

  void FireTimer() { timer_->Fire(); }

  size_t num_disconnect_calls() const {
    return fake_connection_manager_->num_disconnect_calls();
  }

  void SetPhoneConnected() {
    fake_connection_manager_->SetStatus(
        secure_channel::ConnectionManager::Status::kConnected);
  }

  void SetPhoneDisconnected() {
    fake_connection_manager_->SetStatus(
        secure_channel::ConnectionManager::Status::kDisconnected);
  }

  void ClearPhoneModel() {
    fake_phone_model_.SetPhoneStatusModel(std::nullopt);
  }

  void SetPhoneModel() {
    fake_phone_model_.SetPhoneStatusModel(CreateFakePhoneStatusModel());
  }

  bool IsTimerRunning() const { return timer_->IsRunning(); }

 private:
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  MutablePhoneModel fake_phone_model_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;
  std::unique_ptr<InvalidConnectionDisconnector>
      invalid_connection_disconnector_;
};

TEST_F(InvalidConnectionDisconnectorTest, DisconnectFlows) {
  // A disconnection should occur when the phone becomes connected and phone
  // status model is empty after grace period.
  ClearPhoneModel();
  EXPECT_FALSE(IsTimerRunning());
  SetPhoneConnected();
  EXPECT_TRUE(IsTimerRunning());
  FireTimer();
  EXPECT_EQ(num_disconnect_calls(), 1U);

  // No disconnection should occur when the phone becomes connected and phone
  // status model is non-empty before grace period.
  EXPECT_FALSE(IsTimerRunning());
  SetPhoneConnected();
  EXPECT_TRUE(IsTimerRunning());
  SetPhoneModel();
  FireTimer();
  EXPECT_EQ(num_disconnect_calls(), 1U);

  // A change in connection status while the timer is running will cause the
  // timer to stop.
  ClearPhoneModel();
  SetPhoneDisconnected();
  EXPECT_FALSE(IsTimerRunning());
  SetPhoneConnected();
  EXPECT_TRUE(IsTimerRunning());
  SetPhoneDisconnected();
  EXPECT_FALSE(IsTimerRunning());
}

}  // namespace phonehub
}  // namespace ash
