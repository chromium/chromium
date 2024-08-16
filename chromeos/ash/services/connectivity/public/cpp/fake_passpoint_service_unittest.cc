// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_service.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_subscription.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom-test-utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::connectivity {

using chromeos::connectivity::mojom::PasspointEventsListener;
using chromeos::connectivity::mojom::PasspointServiceAsyncWaiter;
using chromeos::connectivity::mojom::PasspointSubscriptionPtr;

namespace {

const char kPasspointId[] = "fake_id";
const char kPasspointFriendlyName[] = "fake_friendly_name";
const char kPasspointProvisioningSource[] = "fake_provisioning_source";
const char kPasspointTrustedCa[] = "fake_ca";
const int64_t kPasspointExpirationEpochMs = 1723071654000;

}  // namespace

class FakePasspointEventsListener
    : public chromeos::connectivity::mojom::PasspointEventsListener {
 public:
  FakePasspointEventsListener() = default;
  ~FakePasspointEventsListener() override = default;
  FakePasspointEventsListener(const FakePasspointEventsListener&) = delete;
  FakePasspointEventsListener& operator=(const FakePasspointEventsListener&) =
      delete;

  mojo::PendingRemote<PasspointEventsListener> GenerateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // chromeos::connectivity::mojom::PasspointEventsListener:
  void OnPasspointSubscriptionAdded(
      PasspointSubscriptionPtr added_passpoint) override {
    passpoint_added_count_++;
  }

  void OnPasspointSubscriptionRemoved(
      PasspointSubscriptionPtr removed_passpoint) override {
    passpoint_removed_count_++;
  }

  size_t passpoint_added_count() const { return passpoint_added_count_; }
  size_t passpoint_removed_count() const { return passpoint_removed_count_; }

 private:
  mojo::Receiver<PasspointEventsListener> receiver_{this};
  size_t passpoint_added_count_ = 0;
  size_t passpoint_removed_count_ = 0;
};

class FakePasspointServiceTest : public testing::Test {
 public:
  FakePasspointServiceTest() = default;
  FakePasspointServiceTest(const FakePasspointServiceTest&) = delete;
  FakePasspointServiceTest& operator=(const FakePasspointServiceTest&) = delete;
  ~FakePasspointServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    if (!FakePasspointService::IsInitialized()) {
      FakePasspointService::Initialize();
    }
  }

  void TearDown() override { FakePasspointService::Shutdown(); }

  void SetupListener() {
    listener_ = std::make_unique<FakePasspointEventsListener>();
    FakePasspointService::Get()->RegisterPasspointListener(
        listener_->GenerateRemote());
  }

  PasspointSubscriptionPtr GetPasspointSubscription(const std::string& id) {
    auto passpoint_service_async_waiter =
        PasspointServiceAsyncWaiter(FakePasspointService::Get());
    PasspointSubscriptionPtr result;
    passpoint_service_async_waiter.GetPasspointSubscription(id, &result);
    return result;
  }

  std::vector<PasspointSubscriptionPtr> ListPasspointSubscriptions() {
    auto passpoint_service_async_waiter =
        chromeos::connectivity::mojom::PasspointServiceAsyncWaiter(
            FakePasspointService::Get());
    std::vector<PasspointSubscriptionPtr> result;
    passpoint_service_async_waiter.ListPasspointSubscriptions(&result);
    return result;
  }

  bool DeletePasspointSubscription(const std::string& id) {
    auto passpoint_service_async_waiter =
        chromeos::connectivity::mojom::PasspointServiceAsyncWaiter(
            FakePasspointService::Get());
    bool result = false;
    passpoint_service_async_waiter.DeletePasspointSubscription(id, &result);
    return result;
  }

  void FlushListenerMojoCallback() { base::RunLoop().RunUntilIdle(); }

  FakePasspointEventsListener* listner() { return listener_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FakePasspointEventsListener> listener_;
};

TEST_F(FakePasspointServiceTest, AddRemoveFakePasspointSubscription) {
  SetupListener();
  EXPECT_EQ(listner()->passpoint_added_count(), 0u);
  EXPECT_EQ(listner()->passpoint_removed_count(), 0u);

  FakePasspointService::Get()->AddFakePasspointSubscription(
      FakePasspointSubscription(
          kPasspointId, kPasspointFriendlyName, kPasspointProvisioningSource,
          kPasspointTrustedCa, kPasspointExpirationEpochMs,
          std::vector<std::string>()));
  FlushListenerMojoCallback();

  EXPECT_EQ(listner()->passpoint_added_count(), 1u);
  EXPECT_EQ(ListPasspointSubscriptions().size(), 1u);
  auto passpoint_subscription = GetPasspointSubscription(kPasspointId);
  EXPECT_TRUE(passpoint_subscription);
  EXPECT_EQ(passpoint_subscription->id, kPasspointId);
  EXPECT_EQ(passpoint_subscription->friendly_name, kPasspointFriendlyName);
  EXPECT_EQ(passpoint_subscription->provisioning_source,
            kPasspointProvisioningSource);
  EXPECT_EQ(passpoint_subscription->trusted_ca, kPasspointTrustedCa);
  EXPECT_EQ(passpoint_subscription->expiration_epoch_ms,
            kPasspointExpirationEpochMs);
  EXPECT_EQ(passpoint_subscription->domains.size(), 0u);
  EXPECT_TRUE(DeletePasspointSubscription(kPasspointId));
  FlushListenerMojoCallback();

  EXPECT_EQ(listner()->passpoint_removed_count(), 1u);
  EXPECT_EQ(ListPasspointSubscriptions().size(), 0u);
}

}  // namespace ash::connectivity
