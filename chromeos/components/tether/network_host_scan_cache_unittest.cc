// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_host_scan_cache.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_host_scan_cache.h"
#include "chromeos/components/tether/host_scan_test_util.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Invoke;

namespace chromeos {

namespace tether {

class NetworkHostScanCacheTest : public testing::Test {
 protected:
  NetworkHostScanCacheTest()
      : test_entries_(host_scan_test_util::CreateTestEntries()) {}

  void SetUp() override {
    helper_.network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    mock_tether_host_response_recorder_ =
        std::make_unique<NiceMock<MockTetherHostResponseRecorder>>();
    device_id_tether_network_guid_map_ =
        std::make_unique<DeviceIdTetherNetworkGuidMap>();

    ON_CALL(*mock_tether_host_response_recorder_,
            GetPreviouslyConnectedHostIds())
        .WillByDefault(Invoke(
            this, &NetworkHostScanCacheTest::GetPreviouslyConnectedHostIds));

    host_scan_cache_ = std::make_unique<NetworkHostScanCache>(
        helper_.network_state_handler(),
        mock_tether_host_response_recorder_.get(),
        device_id_tether_network_guid_map_.get());

    // To track what is expected to be contained in the cache, maintain a
    // FakeHostScanCache in memory and update it alongside |host_scan_cache_|.
    // Use a std::vector to track which device IDs correspond to devices whose
    // Tether networks' HasConnectedToHost fields are expected to be set.
    expected_cache_ = std::make_unique<FakeHostScanCache>();
    has_connected_to_host_device_ids_.clear();
  }

  void TearDown() override {}

  std::vector<std::string> GetPreviouslyConnectedHostIds() const {
    return has_connected_to_host_device_ids_;
  }

  void SetHasConnectedToHost(const std::string& tether_network_guid) {
    has_connected_to_host_device_ids_.push_back(
        device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
            tether_network_guid));
    mock_tether_host_response_recorder_
        ->NotifyObserversPreviouslyConnectedHostIdsChanged();
  }

  void SetHostScanResult(const HostScanCacheEntry& entry) {
    host_scan_cache_->SetHostScanResult(entry);
    expected_cache_->SetHostScanResult(entry);
  }

  void RemoveHostScanResult(const std::string& tether_network_guid) {
    host_scan_cache_->RemoveHostScanResult(tether_network_guid);
    expected_cache_->RemoveHostScanResult(tether_network_guid);
  }

  bool HasConnectedToHost(const std::string& tether_network_guid) {
    return base::Contains(has_connected_to_host_device_ids_,
                          tether_network_guid);
  }

  // Verifies that the information present in |expected_cache_| and
  // |has_connected_to_host_device_ids_| mirrors what |host_scan_cache_| has set
  // in NetworkStateHandler.
  void VerifyCacheMatchesNetworkStack(size_t expected_size) {
    EXPECT_EQ(expected_size, expected_cache_->size());
    EXPECT_EQ(expected_cache_->GetTetherGuidsInCache(),
              host_scan_cache_->GetTetherGuidsInCache());

    for (auto& it : expected_cache_->cache()) {
      const std::string tether_network_guid = it.first;
      const HostScanCacheEntry& entry = it.second;

      // Ensure that each entry in |expected_cache_| matches the
      // corresponding entry in NetworkStateHandler.
      const NetworkState* tether_network_state =
          helper_.network_state_handler()->GetNetworkStateFromGuid(
              tether_network_guid);
      ASSERT_TRUE(tether_network_state);
      EXPECT_EQ(entry.device_name, tether_network_state->name());
      EXPECT_EQ(entry.carrier, tether_network_state->tether_carrier());
      EXPECT_EQ(entry.battery_percentage,
                tether_network_state->battery_percentage());
      EXPECT_EQ(entry.signal_strength, tether_network_state->signal_strength());
      EXPECT_EQ(HasConnectedToHost(tether_network_guid),
                tether_network_state->tether_has_connected_to_host());
    }
  }

  const base::test::TaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{true /* use_default_devices_and_services */};
  const std::unordered_map<std::string, HostScanCacheEntry> test_entries_;

  std::unique_ptr<NiceMock<MockTetherHostResponseRecorder>>
      mock_tether_host_response_recorder_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;

  std::vector<std::string> has_connected_to_host_device_ids_;
  std::unique_ptr<FakeHostScanCache> expected_cache_;

  std::unique_ptr<NetworkHostScanCache> host_scan_cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkHostScanCacheTest);
};

TEST_F(NetworkHostScanCacheTest, TestSetAndRemove) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyCacheMatchesNetworkStack(1u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid1));
  VerifyCacheMatchesNetworkStack(2u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid2));
  VerifyCacheMatchesNetworkStack(3u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid3));
  VerifyCacheMatchesNetworkStack(4u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  VerifyCacheMatchesNetworkStack(3u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid1);
  VerifyCacheMatchesNetworkStack(2u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid2);
  VerifyCacheMatchesNetworkStack(1u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid3);
  VerifyCacheMatchesNetworkStack(0u /* expected_size */);
}

TEST_F(NetworkHostScanCacheTest, TestSetScanResultThenUpdateAndRemove) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyCacheMatchesNetworkStack(1u /* expected_size */);

  // Change the fields for tether network with GUID |kTetherGuid0| to the
  // fields corresponding to |kTetherGuid1|.
  SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(host_scan_test_util::kTetherGuid0)
           .SetDeviceName(host_scan_test_util::kTetherDeviceName0)
           .SetCarrier(host_scan_test_util::kTetherCarrier1)
           .SetBatteryPercentage(host_scan_test_util::kTetherBatteryPercentage1)
           .SetSignalStrength(host_scan_test_util::kTetherSignalStrength1)
           .SetSetupRequired(host_scan_test_util::kTetherSetupRequired1)
           .Build());
  VerifyCacheMatchesNetworkStack(1u /* expected_size */);

  // Now, remove that result.
  RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  VerifyCacheMatchesNetworkStack(0u /* expected_size */);
}

TEST_F(NetworkHostScanCacheTest, TestHasConnectedToHost) {
  // Before the test starts, set device 0 as having already connected.
  SetHasConnectedToHost(host_scan_test_util::kTetherGuid0);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyCacheMatchesNetworkStack(1u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid1));
  VerifyCacheMatchesNetworkStack(2u /* expected_size */);

  // Simulate a connection to device 1.
  SetHasConnectedToHost(host_scan_test_util::kTetherGuid1);
  VerifyCacheMatchesNetworkStack(2u /* expected_size */);
}

}  // namespace tether

}  // namespace chromeos
