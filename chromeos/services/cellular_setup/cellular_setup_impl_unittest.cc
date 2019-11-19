// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/cellular_setup_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/cellular_setup/cellular_setup_base.h"
#include "chromeos/services/cellular_setup/cellular_setup_impl.h"
#include "chromeos/services/cellular_setup/fake_ota_activator.h"
#include "chromeos/services/cellular_setup/ota_activator_impl.h"
#include "chromeos/services/cellular_setup/public/cpp/fake_activation_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace cellular_setup {

namespace {

class FakeOtaActivatorFactory : public OtaActivatorImpl::Factory {
 public:
  FakeOtaActivatorFactory() = default;
  ~FakeOtaActivatorFactory() override = default;

  std::vector<FakeOtaActivator*>& created_instances() {
    return created_instances_;
  }

 private:
  // OtaActivatorImpl::Factory:
  std::unique_ptr<OtaActivator> BuildInstance(
      mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
      base::OnceClosure on_finished_callback,
      NetworkStateHandler* network_state_handler,
      NetworkConnectionHandler* network_connection_handler,
      NetworkActivationHandler* network_activation_handler,
      scoped_refptr<base::TaskRunner> task_runner) override {
    EXPECT_TRUE(activation_delegate);
    EXPECT_TRUE(network_state_handler);
    EXPECT_TRUE(network_connection_handler);
    EXPECT_TRUE(network_activation_handler);
    EXPECT_TRUE(task_runner);

    auto fake_ota_activator =
        std::make_unique<FakeOtaActivator>(std::move(on_finished_callback));
    created_instances_.push_back(fake_ota_activator.get());

    return fake_ota_activator;
  }

  std::vector<FakeOtaActivator*> created_instances_;

  DISALLOW_COPY_AND_ASSIGN(FakeOtaActivatorFactory);
};

}  // namespace

class CellularSetupImplTest : public testing::Test {
 protected:
  CellularSetupImplTest() = default;
  ~CellularSetupImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    OtaActivatorImpl::Factory::SetFactoryForTesting(
        &fake_ota_activator_factory_);
    shill_clients::InitializeFakes();
    NetworkHandler::Initialize();
    cellular_setup_ = std::make_unique<CellularSetupImpl>();
  }

  void TearDown() override {
    cellular_setup_.reset();
    NetworkHandler::Shutdown();
    shill_clients::Shutdown();
    OtaActivatorImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void CallStartActivation(FakeActivationDelegate* fake_activation_delegate) {
    size_t num_before_call = num_carrier_portal_handlers_received_;
    EXPECT_EQ(num_before_call,
              fake_ota_activator_factory_.created_instances().size());

    base::RunLoop run_loop;
    cellular_setup_->StartActivation(
        fake_activation_delegate->GenerateRemote(),
        base::BindOnce(&CellularSetupImplTest::OnCarrierPortalHandlerReceived,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(num_before_call + 1u, num_carrier_portal_handlers_received_);
    EXPECT_EQ(num_before_call + 1u,
              fake_ota_activator_factory_.created_instances().size());

    fake_ota_activator_factory_.created_instances()[num_before_call]
        ->InvokeOnFinishedCallback();
  }

 private:
  void OnCarrierPortalHandlerReceived(
      base::OnceClosure quit_closure,
      mojo::PendingRemote<mojom::CarrierPortalHandler> carrier_portal_handler) {
    ++num_carrier_portal_handlers_received_;
    std::move(quit_closure).Run();
  }

  base::test::TaskEnvironment task_environment_;
  FakeOtaActivatorFactory fake_ota_activator_factory_;

  std::unique_ptr<CellularSetupBase> cellular_setup_;

  size_t num_carrier_portal_handlers_received_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(CellularSetupImplTest);
};

TEST_F(CellularSetupImplTest, StartActivation_SingleAttempt) {
  auto fake_activation_delegate = std::make_unique<FakeActivationDelegate>();
  CallStartActivation(fake_activation_delegate.get());
}

TEST_F(CellularSetupImplTest, StartActivation_MultipleAttempts) {
  auto fake_activation_delegate_1 = std::make_unique<FakeActivationDelegate>();
  CallStartActivation(fake_activation_delegate_1.get());

  auto fake_activation_delegate_2 = std::make_unique<FakeActivationDelegate>();
  CallStartActivation(fake_activation_delegate_2.get());
}

}  // namespace cellular_setup

}  // namespace chromeos
