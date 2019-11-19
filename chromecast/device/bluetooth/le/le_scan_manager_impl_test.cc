// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/le_scan_manager_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "chromecast/device/bluetooth/shlib/mock_le_scanner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromecast {
namespace bluetooth {

namespace {

const bluetooth_v2_shlib::Addr kTestAddr1 = {
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
const bluetooth_v2_shlib::Addr kTestAddr2 = {
    {0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B}};

// This hack is needed because base::BindOnce does not support capture lambdas.
template <typename T>
void CopyResult(T* out, T in) {
  *out = std::move(in);
}

class MockLeScanManagerObserver : public LeScanManager::Observer {
 public:
  MOCK_METHOD1(OnScanEnableChanged, void(bool enabled));
  MOCK_METHOD1(OnNewScanResult, void(LeScanResult result));
};

class LeScanManagerTest : public ::testing::Test {
 protected:
  LeScanManagerTest()
      : io_task_runner_(base::CreateSingleThreadTaskRunner(
            {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
             base::MayBlock()})),
        le_scan_manager_(&le_scanner_) {
    le_scan_manager_.Initialize(io_task_runner_);
    le_scan_manager_.AddObserver(&mock_observer_);
    task_environment_.RunUntilIdle();
  }
  ~LeScanManagerTest() override {
    le_scan_manager_.RemoveObserver(&mock_observer_);
    le_scan_manager_.Finalize();
  }

  bluetooth_v2_shlib::LeScanner::Delegate* delegate() {
    return &le_scan_manager_;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  bluetooth_v2_shlib::MockLeScanner le_scanner_;
  LeScanManagerImpl le_scan_manager_;
  MockLeScanManagerObserver mock_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LeScanManagerTest);
};

}  // namespace

TEST_F(LeScanManagerTest, TestEnableDisableScan) {
  std::unique_ptr<LeScanManager::ScanHandle> scan_handle;

  // Enable the LE scan. We expect the observer to be updated and to get a scan
  // handle.
  EXPECT_CALL(le_scanner_, StartScan()).WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(true));
  le_scan_manager_.RequestScan(base::BindOnce(
      &CopyResult<std::unique_ptr<LeScanManager::ScanHandle>>, &scan_handle));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(scan_handle);

  // After deleting the last handle, we expect scan to be disabled.
  EXPECT_CALL(le_scanner_, StopScan()).WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(false));
  scan_handle.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(LeScanManagerTest, TestPauseRestartScan) {
  std::unique_ptr<LeScanManager::ScanHandle> scan_handle;

  // Don't call StartScan or StopScan if there is no handle.
  EXPECT_CALL(le_scanner_, StopScan()).Times(0);
  le_scan_manager_.PauseScan();
  EXPECT_CALL(le_scanner_, StartScan()).Times(0);
  le_scan_manager_.RestartScan();
  task_environment_.RunUntilIdle();

  // Create a handle.
  EXPECT_CALL(le_scanner_, StartScan()).WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(true));
  le_scan_manager_.RequestScan(base::BindOnce(
      &CopyResult<std::unique_ptr<LeScanManager::ScanHandle>>, &scan_handle));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(scan_handle);

  // Pause scan, we shouldn't declare scan is disabled.
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(_)).Times(0);
  EXPECT_CALL(le_scanner_, StopScan()).WillOnce(Return(true));
  le_scan_manager_.PauseScan();
  task_environment_.RunUntilIdle();

  // Restart scan.
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(_)).Times(0);
  EXPECT_CALL(le_scanner_, StartScan()).WillOnce(Return(true));
  le_scan_manager_.RestartScan();
  task_environment_.RunUntilIdle();

  // Delete the handle.
  EXPECT_CALL(le_scanner_, StopScan()).WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(false));
  scan_handle.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(LeScanManagerTest, TestMultipleHandles) {
  static constexpr int kNumHandles = 20;

  std::vector<std::unique_ptr<LeScanManager::ScanHandle>> scan_handles;

  EXPECT_CALL(le_scanner_, StartScan()).WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(true));
  for (int i = 0; i < kNumHandles; ++i) {
    std::unique_ptr<LeScanManager::ScanHandle> handle;
    le_scan_manager_.RequestScan(base::BindOnce(
        &CopyResult<std::unique_ptr<LeScanManager::ScanHandle>>, &handle));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(handle);
    scan_handles.push_back(std::move(handle));
  }

  EXPECT_CALL(le_scanner_, StopScan()).Times(0);
  for (int i = 0; i < kNumHandles - 1; ++i) {
    scan_handles.pop_back();
    task_environment_.RunUntilIdle();
  }

  // After deleting the last handle, we expect scan to be disabled.
  EXPECT_CALL(le_scanner_, StopScan()).WillOnce(Return(true));
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(false));
  scan_handles.pop_back();
  task_environment_.RunUntilIdle();
}

TEST_F(LeScanManagerTest, TestGetScanResultsEmpty) {
  std::vector<LeScanResult> results;

  // Get asynchronous scan results. The result should be empty.
  le_scan_manager_.GetScanResults(
      base::BindOnce(&CopyResult<std::vector<LeScanResult>>, &results));

  task_environment_.RunUntilIdle();
  ASSERT_EQ(0u, results.size());
}

TEST_F(LeScanManagerTest, TestEnableScanFails) {
  std::unique_ptr<LeScanManager::ScanHandle> scan_handle;

  // Observer should not be notified.
  EXPECT_CALL(mock_observer_, OnScanEnableChanged(true)).Times(0);

  EXPECT_CALL(le_scanner_, StartScan()).WillOnce(Return(false));
  le_scan_manager_.RequestScan(base::BindOnce(
      &CopyResult<std::unique_ptr<LeScanManager::ScanHandle>>, &scan_handle));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(scan_handle);
}

TEST_F(LeScanManagerTest, TestGetScanResults) {
  // Simulate some scan results.
  bluetooth_v2_shlib::LeScanner::ScanResult raw_scan_result;
  raw_scan_result.addr = kTestAddr1;
  raw_scan_result.rssi = 1234;

  EXPECT_CALL(mock_observer_, OnNewScanResult(_));
  delegate()->OnScanResult(raw_scan_result);
  task_environment_.RunUntilIdle();

  std::vector<LeScanResult> results;
  // Get asynchronous scan results.
  le_scan_manager_.GetScanResults(
      base::BindOnce(&CopyResult<std::vector<LeScanResult>>, &results));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, results.size());
  ASSERT_EQ(kTestAddr1, results[0].addr);
  ASSERT_EQ(1234, results[0].rssi);
}

TEST_F(LeScanManagerTest, TestGetScanResultsWithService) {
  EXPECT_CALL(mock_observer_, OnNewScanResult(_)).Times(2);

  // Add a scan result with service 0x4444.
  bluetooth_v2_shlib::LeScanner::ScanResult raw_scan_result;
  raw_scan_result.addr = kTestAddr1;
  raw_scan_result.adv_data = {0x03, 0x02, 0x44, 0x44};
  raw_scan_result.rssi = 1234;
  delegate()->OnScanResult(raw_scan_result);

  // Add a scan result with service 0x5555.
  raw_scan_result.addr = kTestAddr2;
  raw_scan_result.adv_data = {0x03, 0x02, 0x55, 0x55};
  raw_scan_result.rssi = 1234;
  delegate()->OnScanResult(raw_scan_result);

  task_environment_.RunUntilIdle();

  // Get asynchronous scan results for results with service 0x4444.
  std::vector<LeScanResult> results;
  le_scan_manager_.GetScanResults(
      base::BindOnce(&CopyResult<std::vector<LeScanResult>>, &results),
      ScanFilter::From16bitUuid(0x4444));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, results.size());
  ASSERT_EQ(kTestAddr1, results[0].addr);
  ASSERT_EQ(std::vector<uint8_t>({0x44, 0x44}),
            results[0].type_to_data[0x02][0]);
  ASSERT_EQ(1234, results[0].rssi);

  // Get asynchronous scan results for results with service 0x5555.
  le_scan_manager_.GetScanResults(
      base::BindOnce(&CopyResult<std::vector<LeScanResult>>, &results),
      ScanFilter::From16bitUuid(0x5555));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, results.size());
  ASSERT_EQ(kTestAddr2, results[0].addr);
  ASSERT_EQ(std::vector<uint8_t>({0x55, 0x55}),
            results[0].type_to_data[0x02][0]);
  ASSERT_EQ(1234, results[0].rssi);

  // Get asynchronous scan results for results with service 0x6666.
  le_scan_manager_.GetScanResults(
      base::BindOnce(&CopyResult<std::vector<LeScanResult>>, &results),
      ScanFilter::From16bitUuid(0x6666));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(0u, results.size());
}

TEST_F(LeScanManagerTest, TestGetScanResultsSortedByRssi) {
  EXPECT_CALL(mock_observer_, OnNewScanResult(_)).Times(3);

  // Add a scan result with service 0x4444.
  bluetooth_v2_shlib::LeScanner::ScanResult raw_scan_result;
  raw_scan_result.addr = kTestAddr1;
  raw_scan_result.adv_data = {0x03, 0x02, 0x44, 0x44};
  raw_scan_result.rssi = 1;
  delegate()->OnScanResult(raw_scan_result);

  // Add a scan result with service 0x5555.
  raw_scan_result.addr = kTestAddr2;
  raw_scan_result.adv_data = {0x03, 0x02, 0x55, 0x55};
  raw_scan_result.rssi = 3;
  delegate()->OnScanResult(raw_scan_result);

  // Add a scan result with service 0x5555.
  raw_scan_result.addr = kTestAddr1;
  raw_scan_result.adv_data = {0x03, 0x02, 0x55, 0x55};
  raw_scan_result.rssi = 2;
  delegate()->OnScanResult(raw_scan_result);

  task_environment_.RunUntilIdle();

  std::vector<LeScanResult> results;
  // Get asynchronous scan results.
  le_scan_manager_.GetScanResults(
      base::BindOnce(&CopyResult<std::vector<LeScanResult>>, &results));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(3u, results.size());
  EXPECT_EQ(kTestAddr2, results[0].addr);
  EXPECT_EQ(3, results[0].rssi);
  EXPECT_EQ(kTestAddr1, results[1].addr);
  EXPECT_EQ(2, results[1].rssi);
  EXPECT_EQ(kTestAddr1, results[2].addr);
  EXPECT_EQ(1, results[2].rssi);
}

TEST_F(LeScanManagerTest, TestOnNewScanResult) {
  LeScanResult result;
  ON_CALL(mock_observer_, OnNewScanResult(_))
      .WillByDefault(
          Invoke([&result](LeScanResult result_in) { result = result_in; }));

  // Add a scan result with service 0x4444.
  bluetooth_v2_shlib::LeScanner::ScanResult raw_scan_result;
  raw_scan_result.addr = kTestAddr1;
  raw_scan_result.adv_data = {0x03, 0x02, 0x44, 0x44};
  raw_scan_result.rssi = 1;
  delegate()->OnScanResult(raw_scan_result);
  task_environment_.RunUntilIdle();

  // Ensure that the observer was notified.
  ASSERT_EQ(kTestAddr1, result.addr);
  ASSERT_EQ(std::vector<uint8_t>({0x44, 0x44}), result.type_to_data[0x02][0]);
  ASSERT_EQ(1, result.rssi);
}

TEST_F(LeScanManagerTest, TestMaxScanResultEntries) {
  EXPECT_CALL(mock_observer_, OnNewScanResult(_))
      .Times(LeScanManagerImpl::kMaxScanResultEntries + 5);

  // Add scan results with different addrs.
  bluetooth_v2_shlib::LeScanner::ScanResult raw_scan_result;
  for (int i = 0; i < LeScanManagerImpl::kMaxScanResultEntries + 5; ++i) {
    uint8_t addr_bit0 = i & 0xFF;
    uint8_t addr_bit1 = (i & 0xFF00) >> 8;
    raw_scan_result.addr = {{addr_bit0, addr_bit1, 0xFF, 0xFF, 0xFF, 0xFF}};
    raw_scan_result.adv_data = {0x03, 0x02, 0x44, 0x44};
    raw_scan_result.rssi = -i;
    delegate()->OnScanResult(raw_scan_result);
  }

  task_environment_.RunUntilIdle();

  std::vector<LeScanResult> results;
  // Get asynchronous scan results.
  le_scan_manager_.GetScanResults(
      base::BindOnce(&CopyResult<std::vector<LeScanResult>>, &results));

  task_environment_.RunUntilIdle();

  // First 5 addresses should have been kicked out.
  ASSERT_EQ(1024u, results.size());
  bluetooth_v2_shlib::Addr test_addr;
  for (int i = 0; i < LeScanManagerImpl::kMaxScanResultEntries; ++i) {
    uint8_t addr_bit0 = (i + 5) & 0xFF;
    uint8_t addr_bit1 = ((i + 5) & 0xFF00) >> 8;
    test_addr = {{addr_bit0, addr_bit1, 0xFF, 0xFF, 0xFF, 0xFF}};
    EXPECT_EQ(test_addr, results[i].addr);
    EXPECT_EQ(-(i + 5), results[i].rssi);
  }
}

}  // namespace bluetooth
}  // namespace chromecast
