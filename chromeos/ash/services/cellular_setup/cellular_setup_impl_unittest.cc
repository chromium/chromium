// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/cellular_setup_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/services/cellular_setup/cellular_setup_base.h"
#include "chromeos/ash/services/cellular_setup/fake_ota_activator.h"
#include "chromeos/ash/services/cellular_setup/ota_activator_impl.h"
#include "chromeos/ash/services/cellular_setup/public/cpp/fake_activation_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cellular_setup {

namespace {

class FakeOtaActivatorFactory : public OtaActivatorImpl::Factory {
 public:
  FakeOtaActivatorFactory() = default;

  FakeOtaActivatorFactory(const FakeOtaActivatorFactory&) = delete;
  FakeOtaActivatorFactory& operator=(const FakeOtaActivatorFactory&) = delete;

  ~FakeOtaActivatorFactory() override = default;

  std::vector<raw_ptr<FakeOtaActivator, VectorExperimental>>&
  created_instances() {
    return created_instances_;
  }

 private:
  // OtaActivatorImpl::Factory:
  std::unique_ptr<OtaActivator> CreateInstance(
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

  std::vector<raw_ptr<FakeOtaActivator, VectorExperimental>> created_instances_;
};

}  // namespace

class CellularSetupImplTest : public testing::Test {
 public:
  CellularSetupImplTest(const CellularSetupImplTest&) = delete;
  CellularSetupImplTest& operator=(const CellularSetupImplTest&) = delete;

 protected:
  CellularSetupImplTest() = default;
  ~CellularSetupImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    OtaActivatorImpl::Factory::SetFactoryForTesting(
        &fake_ota_activator_factory_);
  }

  void TearDown() override {
    OtaActivatorImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void CallStartActivation(FakeActivationDelegate* fake_activation_delegate) {
    size_t num_before_call = num_carrier_portal_handlers_received_;
    EXPECT_EQ(num_before_call,
              fake_ota_activator_factory_.created_instances().size());

    base::RunLoop run_loop;
    cellular_setup_.StartActivation(
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
  NetworkHandlerTestHelper network_handler_test_helper_;
  FakeOtaActivatorFactory fake_ota_activator_factory_;

  CellularSetupImpl cellular_setup_;

  size_t num_carrier_portal_handlers_received_ = 0u;
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

}  // namespace ash::cellular_setup
