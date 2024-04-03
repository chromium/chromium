// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host_fetcher_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

namespace {

class TestObserver : public TetherHostFetcher::Observer {
 public:
  TestObserver() = default;
  virtual ~TestObserver() = default;

  size_t num_updates() { return num_updates_; }

  // TetherHostFetcher::Observer:
  void OnTetherHostUpdated() override { ++num_updates_; }

 private:
  size_t num_updates_ = 0;
};

// This should be identical to TetherHostSource in tether_host_fetcher_impl.cc.
enum class TetherHostSource {
  UNKNOWN,
  MULTIDEVICE_SETUP_CLIENT,
  DEVICE_SYNC_CLIENT,
  REMOTE_DEVICE_PROVIDER
};

}  // namespace

class TetherHostFetcherImplTest : public testing::Test {
 public:
  TetherHostFetcherImplTest() {}

  TetherHostFetcherImplTest(const TetherHostFetcherImplTest&) = delete;
  TetherHostFetcherImplTest& operator=(const TetherHostFetcherImplTest&) =
      delete;

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
  }

  void InitializeTest() {
    tether_host_fetcher_ = TetherHostFetcherImpl::Factory::Create(
        fake_device_sync_client_.get(), fake_multidevice_setup_client_.get());

    fake_device_sync_client_->NotifyReady();
    test_observer_ = std::make_unique<TestObserver>();
    tether_host_fetcher_->AddObserver(test_observer_.get());
  }

  void VerifyTetherHost(
      std::optional<multidevice::RemoteDeviceRef> expected_device) {
    std::optional<multidevice::RemoteDeviceRef> tether_host =
        tether_host_fetcher_->GetTetherHost();
    if (expected_device) {
      EXPECT_EQ(expected_device, tether_host);
    } else {
      EXPECT_EQ(std::nullopt, tether_host);
    }
  }

  void SetSyncedDevice(
      std::optional<multidevice::RemoteDeviceRef> remote_device) {
    if (!remote_device.has_value()) {
      fake_device_sync_client_->set_synced_devices(
          multidevice::RemoteDeviceRefList{});
      fake_multidevice_setup_client_->SetHostStatusWithDevice(
          std::make_pair(multidevice_setup::mojom::HostStatus::kNoEligibleHosts,
                         std::nullopt /* host_device */));
      fake_multidevice_setup_client_->SetFeatureState(
          multidevice_setup::mojom::Feature::kInstantTethering,
          multidevice_setup::mojom::FeatureState::
              kUnavailableNoVerifiedHost_NoEligibleHosts);
      return;
    }

    fake_device_sync_client_->set_synced_devices(
        multidevice::RemoteDeviceRefList{*remote_device});
    fake_multidevice_setup_client_->SetHostStatusWithDevice(std::make_pair(
        multidevice_setup::mojom::HostStatus::kHostVerified, remote_device));
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kInstantTethering,
        multidevice_setup::mojom::FeatureState::kEnabledByUser);
  }

  void NotifyNewDevicesSynced() {
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

  std::unique_ptr<TetherHostFetcher> tether_host_fetcher_;
};

TEST_F(TetherHostFetcherImplTest, TestHasSyncedTetherHosts) {
  multidevice::RemoteDeviceRef test_device =
      multidevice::CreateRemoteDeviceRefForTest();
  SetSyncedDevice(test_device);
  InitializeTest();
  VerifyTetherHost(test_device);

  EXPECT_EQ(0u, test_observer_->num_updates());

  // Update the list of devices to be empty.
  SetSyncedDevice(std::nullopt);
  NotifyNewDevicesSynced();
  VerifyTetherHost(std::nullopt);
  EXPECT_EQ(1u, test_observer_->num_updates());

  // Notify that the list has changed, even though it hasn't. There should be
  // no update.
  NotifyNewDevicesSynced();
  VerifyTetherHost(std::nullopt);
  EXPECT_EQ(1u, test_observer_->num_updates());

  // Update the list to include device 0 only.
  SetSyncedDevice(test_device);
  NotifyNewDevicesSynced();
  VerifyTetherHost(test_device);
  EXPECT_EQ(2u, test_observer_->num_updates());

  // Notify that the list has changed, even though it hasn't. There should be
  // no update.
  NotifyNewDevicesSynced();
  VerifyTetherHost(test_device);
  EXPECT_EQ(2u, test_observer_->num_updates());
}

}  // namespace ash::tether
