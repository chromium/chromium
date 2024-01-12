// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/remote_device_life_cycle_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/proximity_auth/messenger.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {

namespace {

// Subclass of RemoteDeviceLifeCycleImpl to make it testable.
class TestableRemoteDeviceLifeCycleImpl : public RemoteDeviceLifeCycleImpl {
 public:
  TestableRemoteDeviceLifeCycleImpl(
      ash::multidevice::RemoteDeviceRef remote_device,
      std::optional<ash::multidevice::RemoteDeviceRef> local_device,
      ash::secure_channel::SecureChannelClient* secure_channel_client)
      : RemoteDeviceLifeCycleImpl(remote_device,
                                  local_device,
                                  secure_channel_client),
        remote_device_(remote_device) {}

  TestableRemoteDeviceLifeCycleImpl(const TestableRemoteDeviceLifeCycleImpl&) =
      delete;
  TestableRemoteDeviceLifeCycleImpl& operator=(
      const TestableRemoteDeviceLifeCycleImpl&) = delete;

  ~TestableRemoteDeviceLifeCycleImpl() override {}

 private:
  const ash::multidevice::RemoteDeviceRef remote_device_;
};

}  // namespace

class ProximityAuthRemoteDeviceLifeCycleImplTest
    : public testing::Test,
      public RemoteDeviceLifeCycle::Observer {
 public:
  ProximityAuthRemoteDeviceLifeCycleImplTest(
      const ProximityAuthRemoteDeviceLifeCycleImplTest&) = delete;
  ProximityAuthRemoteDeviceLifeCycleImplTest& operator=(
      const ProximityAuthRemoteDeviceLifeCycleImplTest&) = delete;

 protected:
  ProximityAuthRemoteDeviceLifeCycleImplTest()
      : test_remote_device_(ash::multidevice::CreateRemoteDeviceRefForTest()),
        test_local_device_(ash::multidevice::CreateRemoteDeviceRefForTest()),
        fake_secure_channel_client_(
            std::make_unique<ash::secure_channel::FakeSecureChannelClient>()),
        life_cycle_(test_remote_device_,
                    test_local_device_,
                    fake_secure_channel_client_.get()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_current_default_handle_(task_runner_) {}

  ~ProximityAuthRemoteDeviceLifeCycleImplTest() override {
    life_cycle_.RemoveObserver(this);
  }

  void CreateFakeConnectionAttempt() {
    auto fake_connection_attempt =
        std::make_unique<ash::secure_channel::FakeConnectionAttempt>();
    fake_connection_attempt_ = fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        test_remote_device_, test_local_device_,
        std::move(fake_connection_attempt));
  }

  void StartLifeCycle() {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::STOPPED, life_cycle_.GetState());
    life_cycle_.AddObserver(this);

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::STOPPED,
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
    life_cycle_.Start();
    task_runner_->RunUntilIdle();
    Mock::VerifyAndClearExpectations(this);

    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());
  }

  void SimulateSuccessfulAuthenticatedConnection() {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
                           RemoteDeviceLifeCycle::State::AUTHENTICATING));

    auto fake_client_channel =
        std::make_unique<ash::secure_channel::FakeClientChannel>();
    auto* fake_client_channel_raw = fake_client_channel.get();
    fake_connection_attempt_->NotifyConnection(std::move(fake_client_channel));

    Mock::VerifyAndClearExpectations(this);

    EXPECT_CALL(*this,
                OnLifeCycleStateChanged(
                    RemoteDeviceLifeCycle::State::AUTHENTICATING,
                    RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
    task_runner_->RunUntilIdle();

    EXPECT_EQ(fake_client_channel_raw, life_cycle_.GetChannel());
    EXPECT_EQ(fake_client_channel_raw,
              life_cycle_.GetMessenger()->GetChannel());
  }

  void SimulateFailureToAuthenticateConnection(
      ash::secure_channel::mojom::ConnectionAttemptFailureReason failure_reason,
      RemoteDeviceLifeCycle::State expected_life_cycle_state) {
    EXPECT_EQ(RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
              life_cycle_.GetState());

    EXPECT_CALL(*this, OnLifeCycleStateChanged(
                           RemoteDeviceLifeCycle::State::FINDING_CONNECTION,
                           expected_life_cycle_state));

    fake_connection_attempt_->NotifyConnectionAttemptFailure(failure_reason);

    EXPECT_EQ(nullptr, life_cycle_.GetChannel());
    EXPECT_EQ(nullptr, life_cycle_.GetMessenger());

    EXPECT_EQ(expected_life_cycle_state, life_cycle_.GetState());
  }

  MOCK_METHOD2(OnLifeCycleStateChanged,
               void(RemoteDeviceLifeCycle::State old_state,
                    RemoteDeviceLifeCycle::State new_state));

  ash::multidevice::RemoteDeviceRef test_remote_device_;
  ash::multidevice::RemoteDeviceRef test_local_device_;
  std::unique_ptr<ash::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  TestableRemoteDeviceLifeCycleImpl life_cycle_;

  raw_ptr<ash::secure_channel::FakeConnectionAttempt, DanglingUntriaged>
      fake_connection_attempt_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      thread_task_runner_current_default_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Success) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateSuccessfulAuthenticatedConnection();
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      ash::secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR /* failure_reason */,
      RemoteDeviceLifeCycle::State::
          AUTHENTICATION_FAILED /* expected_life_cycle_state */);
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure_BluetoothNotPresent) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      ash::secure_channel::mojom::ConnectionAttemptFailureReason::
          ADAPTER_NOT_PRESENT /* failure_reason */,
      RemoteDeviceLifeCycle::State::STOPPED /* expected_life_cycle_state */);
}

TEST_F(ProximityAuthRemoteDeviceLifeCycleImplTest,
       MultiDeviceApiEnabled_Failure_BluetoothNotPowered) {
  CreateFakeConnectionAttempt();

  StartLifeCycle();
  SimulateFailureToAuthenticateConnection(
      ash::secure_channel::mojom::ConnectionAttemptFailureReason::
          ADAPTER_DISABLED /* failure_reason */,
      RemoteDeviceLifeCycle::State::STOPPED /* expected_life_cycle_state */);
}

}  // namespace proximity_auth
