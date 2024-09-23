// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/keep_alive_scheduler.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/fake_host_scan_cache.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";
const char kRemoteDevicePublicKey[] = "remoteDevicePublicKey";
const char kTestCellProvider[] = "cellProvider";

class OperationDeletedHandler {
 public:
  virtual void OnOperationDeleted() = 0;
};

}  // namespace

namespace ash::tether {

class FakeKeepAliveOperation : public KeepAliveOperation {
 public:
  FakeKeepAliveOperation(
      const TetherHost& tether_host,
      raw_ptr<HostConnection::Factory> host_connection_factory,
      OperationDeletedHandler* handler)
      : KeepAliveOperation(tether_host, host_connection_factory),
        tether_host_(tether_host),
        handler_(handler) {}

  ~FakeKeepAliveOperation() override { handler_->OnOperationDeleted(); }

  void SendOperationFinishedEvent(std::unique_ptr<DeviceStatus> device_status) {
    device_status_ = std::move(device_status);
    OnOperationFinished();
  }

  const TetherHost& get_tether_host() { return tether_host_; }

 private:
  TetherHost tether_host_;
  raw_ptr<OperationDeletedHandler> handler_;
};

class FakeKeepAliveOperationFactory final : public KeepAliveOperation::Factory,
                                            public OperationDeletedHandler {
 public:
  FakeKeepAliveOperationFactory()
      : num_created_(0), num_deleted_(0), last_created_(nullptr) {}
  ~FakeKeepAliveOperationFactory() override = default;

  uint32_t num_created() { return num_created_; }

  uint32_t num_deleted() { return num_deleted_; }

  FakeKeepAliveOperation* last_created() { return last_created_; }

  void OnOperationDeleted() override {
    num_deleted_++;
    last_created_ = nullptr;
  }

 protected:
  std::unique_ptr<KeepAliveOperation> CreateInstance(
      const TetherHost& tether_host,
      raw_ptr<HostConnection::Factory> host_connection_factory) override {
    num_created_++;
    last_created_ =
        new FakeKeepAliveOperation(tether_host, host_connection_factory, this);
    return base::WrapUnique(last_created_.get());
  }

 private:
  uint32_t num_created_;
  uint32_t num_deleted_;
  raw_ptr<FakeKeepAliveOperation> last_created_;
};

class KeepAliveSchedulerTest : public testing::Test {
 public:
  KeepAliveSchedulerTest(const KeepAliveSchedulerTest&) = delete;
  KeepAliveSchedulerTest& operator=(const KeepAliveSchedulerTest&) = delete;

 protected:
  KeepAliveSchedulerTest()
      : test_remote_device_(multidevice::RemoteDeviceRefBuilder()
                                .SetPublicKey(kRemoteDevicePublicKey)
                                .Build()) {}

  void SetUp() override {
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();
    fake_active_host_ = std::make_unique<FakeActiveHost>();
    fake_host_scan_cache_ = std::make_unique<FakeHostScanCache>();
    device_id_tether_network_guid_map_ =
        std::make_unique<DeviceIdTetherNetworkGuidMap>();

    fake_operation_factory_ =
        base::WrapUnique(new FakeKeepAliveOperationFactory());
    KeepAliveOperation::Factory::SetFactoryForTesting(
        fake_operation_factory_.get());

    scheduler_ = base::WrapUnique(new KeepAliveScheduler(
        fake_host_connection_factory_.get(), fake_active_host_.get(),
        fake_host_scan_cache_.get(), device_id_tether_network_guid_map_.get(),
        std::make_unique<base::MockRepeatingTimer>()));
  }

  void VerifyTimerRunning(bool is_running) {
    EXPECT_EQ(is_running, GetSchedulerTimer()->IsRunning());

    if (is_running) {
      EXPECT_EQ(base::Minutes(KeepAliveScheduler::kKeepAliveIntervalMinutes),
                GetSchedulerTimer()->GetCurrentDelay());
    }
  }

  void SendOperationFinishedEventFromLastCreatedOperation(
      const std::string& cell_provider,
      int battery_percentage,
      int connection_strength) {
    fake_operation_factory_->last_created()->SendOperationFinishedEvent(
        std::make_unique<DeviceStatus>(CreateTestDeviceStatus(
            cell_provider, battery_percentage, connection_strength)));
  }

  void VerifyCacheUpdated(multidevice::RemoteDeviceRef remote_device,
                          const std::string& carrier,
                          int battery_percentage,
                          int signal_strength) {
    const HostScanCacheEntry* entry = fake_host_scan_cache_->GetCacheEntry(
        device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
            remote_device.GetDeviceId()));
    ASSERT_TRUE(entry);
    EXPECT_EQ(carrier, entry->carrier);
    EXPECT_EQ(battery_percentage, entry->battery_percentage);
    EXPECT_EQ(signal_strength, entry->signal_strength);
  }

  base::MockRepeatingTimer* GetSchedulerTimer() {
    return static_cast<base::MockRepeatingTimer*>(scheduler_->timer_.get());
  }

  base::test::TaskEnvironment task_environment_;

  multidevice::RemoteDeviceRef test_remote_device_;
  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeHostScanCache> fake_host_scan_cache_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  // raw_ptr<base::MockRepeatingTimer> GetSchedulerTimer();

  std::unique_ptr<FakeKeepAliveOperationFactory> fake_operation_factory_;

  std::unique_ptr<KeepAliveScheduler> scheduler_;
};

TEST_F(KeepAliveSchedulerTest, TestSendTickle_OneActiveHost) {
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/false);

  // Start connecting to a device. No operation should be started.
  fake_active_host_->SetActiveHostConnecting(test_remote_device_.GetDeviceId(),
                                             std::string(kTetherNetworkGuid));
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/false);

  // Connect to the device; the operation should be started.
  fake_active_host_->SetActiveHostConnected(test_remote_device_.GetDeviceId(),
                                            std::string(kTetherNetworkGuid),
                                            std::string(kWifiNetworkGuid));
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_remote_device_, fake_operation_factory_->last_created()
                                     ->get_tether_host()
                                     .remote_device_ref());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/true);

  // Ensure that once the operation is finished, it is deleted.
  SendOperationFinishedEventFromLastCreatedOperation(
      kTestCellProvider, /*battery_percentage=*/50, /*connection_strength=*/2);
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/true);
  VerifyCacheUpdated(test_remote_device_, kTestCellProvider,
                     /*battery_percentage=*/50, /*signal_strength=*/50);

  // Fire the timer; this should result in tickle #2 being sent.
  GetSchedulerTimer()->Fire();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_remote_device_, fake_operation_factory_->last_created()
                                     ->get_tether_host()
                                     .remote_device_ref());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/true);

  // Finish tickle operation #2.
  SendOperationFinishedEventFromLastCreatedOperation(
      kTestCellProvider, /*battery_percentage=*/40, /*connection_strength=*/3);
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/true);
  VerifyCacheUpdated(test_remote_device_, kTestCellProvider,
                     /*battery_percentage=*/40, /*signal_strength=*/75);

  // Fire the timer; this should result in tickle #3 being sent.
  GetSchedulerTimer()->Fire();
  EXPECT_EQ(3u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_remote_device_, fake_operation_factory_->last_created()
                                     ->get_tether_host()
                                     .remote_device_ref());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/true);

  // Finish tickler operation #3. This time, simulate a failure to receive a
  // DeviceStatus back.
  fake_operation_factory_->last_created()->SendOperationFinishedEvent(nullptr);
  EXPECT_EQ(3u, fake_operation_factory_->num_created());
  EXPECT_EQ(3u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/true);

  // The same data returned by tickle #2 should be present.
  VerifyCacheUpdated(test_remote_device_, kTestCellProvider,
                     /*battery_percentage=*/40, /*signal_strength=*/75);

  // Disconnect that device.
  fake_active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(3u, fake_operation_factory_->num_created());
  EXPECT_EQ(3u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(/*is_running=*/false);
}

}  // namespace ash::tether
