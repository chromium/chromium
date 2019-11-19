// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/services/cellular_setup/cellular_setup_impl.h"
#include "chromeos/services/cellular_setup/public/cpp/fake_activation_delegate.h"
#include "chromeos/services/cellular_setup/public/cpp/fake_carrier_portal_handler.h"
#include "chromeos/services/cellular_setup/public/cpp/fake_cellular_setup.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace cellular_setup {

namespace {

using CarrierPortalHandlerPair =
    std::pair<mojo::Remote<mojom::CarrierPortalHandler>,
              FakeCarrierPortalHandler*>;

const char kTestPaymentUrl[] = "testPaymentUrl";
const char kTestPaymentPostData[] = "testPaymentPostData";
const char kTestCarrier[] = "testCarrier";
const char kTestMeid[] = "testMeid";
const char kTestImei[] = "testImei";
const char kTestMdn[] = "testMdn";

}  // namespace

class CellularSetupServiceTest : public testing::Test {
 protected:
  CellularSetupServiceTest() = default;
  ~CellularSetupServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    service_ = std::make_unique<FakeCellularSetup>();
    service_->BindReceiver(cellular_setup_remote_.BindNewPipeAndPassReceiver());
    cellular_setup_remote_.FlushForTesting();
  }

  // Calls StartActivation() and returns the fake CarrierPortalHandler and its
  // associated mojo::Remote<>.
  CarrierPortalHandlerPair CallStartActivation(
      FakeActivationDelegate* fake_activation_delegate) {
    std::vector<std::unique_ptr<FakeCellularSetup::StartActivationInvocation>>&
        start_activation_invocations =
            fake_cellular_setup()->start_activation_invocations();
    size_t num_args_before_call = start_activation_invocations.size();
    base::RunLoop run_loop;

    // Make StartActivation() call and propagate it to the service.
    cellular_setup_remote_->StartActivation(
        fake_activation_delegate->GenerateRemote(),
        base::BindOnce(&CellularSetupServiceTest::OnStartActivationResult,
                       base::Unretained(this), run_loop.QuitClosure()));
    cellular_setup_remote_.FlushForTesting();

    // Verify that the call was made successfully.
    EXPECT_EQ(num_args_before_call + 1u, start_activation_invocations.size());

    // Execute the callback and retrieve the returned CarrierPortalHandler.
    FakeCarrierPortalHandler* fake_carrier_portal_observer =
        start_activation_invocations.back()->ExecuteCallback();
    run_loop.RunUntilIdle();

    EXPECT_TRUE(last_carrier_portal_observer_);
    CarrierPortalHandlerPair observer_pair = std::make_pair(
        std::move(last_carrier_portal_observer_), fake_carrier_portal_observer);
    last_carrier_portal_observer_.reset();

    return observer_pair;
  }

  // Calls OnActivationStarted() for the provided ActivationDelegate, passing
  // test metadata to represent the device. |fake_activation_delegate| must
  // correspond to the delegate provided to the most recent call to
  // CallStartActivation().
  void NotifyLastDelegateThatActivationStarted(
      FakeActivationDelegate* fake_activation_delegate) {
    const std::vector<mojom::CellularMetadataPtr>& cellular_metadata_list =
        fake_activation_delegate->cellular_metadata_list();
    size_t num_elements_before_call = cellular_metadata_list.size();

    GetLastActivationDelegate()->OnActivationStarted(
        mojom::CellularMetadata::New(GURL(kTestPaymentUrl),
                                     kTestPaymentPostData, kTestCarrier,
                                     kTestMeid, kTestImei, kTestMdn));
    GetLastActivationDelegate().FlushForTesting();

    ASSERT_EQ(num_elements_before_call + 1u, cellular_metadata_list.size());
    EXPECT_EQ(GURL(kTestPaymentUrl),
              cellular_metadata_list.back()->payment_url);
    EXPECT_EQ(kTestCarrier, cellular_metadata_list.back()->carrier);
    EXPECT_EQ(kTestMeid, cellular_metadata_list.back()->meid);
    EXPECT_EQ(kTestImei, cellular_metadata_list.back()->imei);
    EXPECT_EQ(kTestMdn, cellular_metadata_list.back()->mdn);
  }

  // Calls OnActivationFinished() for the provided ActivationDelegate, passing
  // |activation_result| to the callback. |fake_activation_delegate| must
  // correspond to the delegate provided to the most recent call to
  // CallStartActivation().
  void NotifyLastDelegateThatActivationFinished(
      mojom::ActivationResult activation_result,
      FakeActivationDelegate* fake_activation_delegate) {
    const std::vector<mojom::ActivationResult>& activation_results =
        fake_activation_delegate->activation_results();
    size_t num_results_before_call = activation_results.size();

    GetLastActivationDelegate()->OnActivationFinished(activation_result);
    GetLastActivationDelegate().FlushForTesting();

    ASSERT_EQ(num_results_before_call + 1u, activation_results.size());
    EXPECT_EQ(activation_result, activation_results.back());
  }

  // Calls OnCarrierPortalStatusChanged() for the provided
  // CarrierPortalStatusObserver and verifies that the status was received.
  void SendCarrierPortalStatusUpdate(
      mojom::CarrierPortalStatus carrier_portal_status,
      CarrierPortalHandlerPair* pair) {
    const std::vector<mojom::CarrierPortalStatus>& status_updates =
        pair->second->status_updates();
    size_t num_updates_before_call = status_updates.size();

    pair->first->OnCarrierPortalStatusChange(carrier_portal_status);
    pair->first.FlushForTesting();

    ASSERT_EQ(num_updates_before_call + 1u, status_updates.size());
    EXPECT_EQ(carrier_portal_status, status_updates.back());
  }

 private:
  void OnStartActivationResult(base::OnceClosure quit_closure,
                               mojo::PendingRemote<mojom::CarrierPortalHandler>
                                   carrier_portal_observer) {
    EXPECT_FALSE(last_carrier_portal_observer_);
    last_carrier_portal_observer_.Bind(std::move(carrier_portal_observer));
    std::move(quit_closure).Run();
  }

  FakeCellularSetup* fake_cellular_setup() { return service_.get(); }

  mojo::Remote<mojom::ActivationDelegate>& GetLastActivationDelegate() {
    return fake_cellular_setup()
        ->start_activation_invocations()
        .back()
        ->activation_delegate();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeCellularSetup> service_;

  mojo::Remote<mojom::CarrierPortalHandler> last_carrier_portal_observer_;

  mojo::Remote<mojom::CellularSetup> cellular_setup_remote_;

  DISALLOW_COPY_AND_ASSIGN(CellularSetupServiceTest);
};

TEST_F(CellularSetupServiceTest, StartActivation_Success) {
  auto fake_activation_delegate = std::make_unique<FakeActivationDelegate>();

  CarrierPortalHandlerPair pair =
      CallStartActivation(fake_activation_delegate.get());
  NotifyLastDelegateThatActivationStarted(fake_activation_delegate.get());

  SendCarrierPortalStatusUpdate(
      mojom::CarrierPortalStatus::kPortalLoadedWithoutPaidUser, &pair);
  SendCarrierPortalStatusUpdate(
      mojom::CarrierPortalStatus::kPortalLoadedAndUserCompletedPayment, &pair);

  NotifyLastDelegateThatActivationFinished(
      mojom::ActivationResult::kSuccessfullyStartedActivation,
      fake_activation_delegate.get());
}

TEST_F(CellularSetupServiceTest, StartActivation_PortalFailsToLoad) {
  auto fake_activation_delegate = std::make_unique<FakeActivationDelegate>();

  CarrierPortalHandlerPair pair =
      CallStartActivation(fake_activation_delegate.get());
  NotifyLastDelegateThatActivationStarted(fake_activation_delegate.get());

  SendCarrierPortalStatusUpdate(mojom::CarrierPortalStatus::kPortalFailedToLoad,
                                &pair);

  NotifyLastDelegateThatActivationFinished(
      mojom::ActivationResult::kFailedToActivate,
      fake_activation_delegate.get());
}

TEST_F(CellularSetupServiceTest, StartActivation_ErrorDuringPayment) {
  auto fake_activation_delegate = std::make_unique<FakeActivationDelegate>();

  CarrierPortalHandlerPair pair =
      CallStartActivation(fake_activation_delegate.get());
  NotifyLastDelegateThatActivationStarted(fake_activation_delegate.get());

  SendCarrierPortalStatusUpdate(
      mojom::CarrierPortalStatus::kPortalLoadedWithoutPaidUser, &pair);
  SendCarrierPortalStatusUpdate(
      mojom::CarrierPortalStatus::kPortalLoadedButErrorOccurredDuringPayment,
      &pair);

  NotifyLastDelegateThatActivationFinished(
      mojom::ActivationResult::kFailedToActivate,
      fake_activation_delegate.get());
}

}  // namespace cellular_setup

}  // namespace chromeos
