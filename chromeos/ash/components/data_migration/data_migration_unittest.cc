// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/data_migration.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/components/data_migration/testing/connection_barrier.h"
#include "chromeos/ash/components/data_migration/testing/fake_nearby_connections.h"
#include "chromeos/ash/components/data_migration/testing/fake_nearby_process_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_migration {
namespace {

constexpr char kRemoteEndpointId[] = "test-remote-endpoint";

// TODO(esum): Move common test harness logic to a dedicated class. Some of this
// is shared with other unit tests.
class DataMigrationTest : public ::testing::Test {
 public:
  DataMigrationTest()
      : nearby_process_manager_(kRemoteEndpointId),
        data_migration_(std::make_unique<NearbyConnectionsManagerImpl>(
            &nearby_process_manager_,
            kServiceId)) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(GetFilePayloadDirectory()));
    home_dir_override_.emplace(base::DIR_HOME, temp_dir_.GetPath());
    nearby_process_manager_.fake_nearby_connections()
        .set_local_to_remote_payload_listener(base::BindRepeating(
            &DataMigrationTest::RespondToCtsMessage, base::Unretained(this)));
  }

  void TearDown() override {
    static_cast<KeyedService*>(&data_migration_)->Shutdown();
  }

  base::FilePath GetFilePayloadDirectory() {
    return temp_dir_.GetPath().Append(kPayloadTargetDir);
  }

  base::FilePath BuildFilePayloadName(int64_t payload_id) {
    return base::FilePath(base::StringPrintf("payload_%ld", payload_id));
  }

  base::FilePath BuildFilePayloadPath(int64_t payload_id) {
    return GetFilePayloadDirectory().Append(BuildFilePayloadName(payload_id));
  }

  void RespondToCtsMessage(
      ::nearby::connections::mojom::PayloadPtr cts_payload) {
    ASSERT_TRUE(cts_payload);
    ASSERT_TRUE(cts_payload->content->is_bytes());
    const std::vector<uint8_t>& cts_bytes =
        cts_payload->content->get_bytes()->bytes;
    int64_t file_payload_id_to_transmit = 0;
    ASSERT_TRUE(
        base::StringToInt64(std::string(cts_bytes.begin(), cts_bytes.end()),
                            &file_payload_id_to_transmit));
    ASSERT_TRUE(
        requested_file_payload_ids_.contains(file_payload_id_to_transmit));
    ASSERT_TRUE(nearby_process_manager_.fake_nearby_connections().SendFile(
        file_payload_id_to_transmit,
        &expected_file_content_[file_payload_id_to_transmit]));
  }

  void SendRts(int64_t file_payload_id) {
    static int64_t g_rts_payload_id_assigner = 1000;
    ASSERT_TRUE(
        nearby_process_manager_.fake_nearby_connections().SendBytesPayload(
            /*payload_id=*/g_rts_payload_id_assigner,
            base::NumberToString(file_payload_id)));
    ++g_rts_payload_id_assigner;
    requested_file_payload_ids_.insert(file_payload_id);
  }

  bool FileIsReady(int64_t payload_id) {
    base::FilePath file_path = BuildFilePayloadPath(payload_id);
    std::optional<int64_t> file_size_in_bytes = base::GetFileSize(file_path);

    return file_size_in_bytes.has_value() &&
           file_size_in_bytes.value() >=
               nearby_process_manager_.fake_nearby_connections()
                   .test_file_size_in_bytes();
  }

  base::flat_set</*payload_id*/ int64_t> requested_file_payload_ids_;
  base::flat_map</*payload_id*/ int64_t, std::vector<uint8_t>>
      expected_file_content_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::optional<base::ScopedPathOverride> home_dir_override_;
  FakeNearbyProcessManager nearby_process_manager_;
  DataMigration data_migration_;
};

TEST_F(DataMigrationTest, CompletesAllFileTransfers) {
  nearby_process_manager_.fake_nearby_connections()
      .set_connection_established_listener(base::BindLambdaForTesting([this]() {
        SendRts(/*file_payload_id=*/1);
        SendRts(/*file_payload_id=*/2);
        SendRts(/*file_payload_id=*/3);
      }));

  data_migration_.StartAdvertising();

  ASSERT_TRUE(base::test::RunUntil([this]() {
    // All expected files have been written to disc.
    return base::ranges::all_of(
        requested_file_payload_ids_,
        [this](int64_t payload_id) { return FileIsReady(payload_id); });
  }));
  // For extra safety, flush any pending tasks to ensure `DataMigration` is
  // completely idle before checking the results.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/1)),
            expected_file_content_.at(/*payload_id=*/1));
  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/2)),
            expected_file_content_.at(/*payload_id=*/2));
  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/3)),
            expected_file_content_.at(/*payload_id=*/3));
}

TEST_F(DataMigrationTest, HandlesDisconnect) {
  // The transfer should be in progress when the remote device is disconnected.
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      FakeNearbyConnections::PayloadStatus::kInProgress, /*payload_id=*/1);
  nearby_process_manager_.fake_nearby_connections()
      .set_connection_established_listener(base::BindLambdaForTesting(
          [this]() { SendRts(/*file_payload_id=*/1); }));

  data_migration_.StartAdvertising();

  // Run until the transfers starts, then disconnect.
  ASSERT_TRUE(base::test::RunUntil([this]() {
    return base::PathExists(BuildFilePayloadPath(/*payload_id=*/1));
  }));

  // The transfer should succeed the second time.
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      FakeNearbyConnections::PayloadStatus::kSuccess, /*payload_id=*/1);

  ASSERT_TRUE(nearby_process_manager_.fake_nearby_connections()
                  .SimulateRemoteDisconnect());

  // Advertising/discovery should be retried and succeed again. This time,
  // the file transfer should succeed.
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return FileIsReady(/*payload_id=*/1); }));
  // For extra safety, flush any pending tasks to ensure `DataMigration` is
  // completely idle before checking the results.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/1)),
            expected_file_content_.at(/*payload_id=*/1));
}

// Verifies that `DataMigration` can be destroyed gracefully mid-transfer. Does
// not have any real expectation other than the code doesn't crash.
TEST_F(DataMigrationTest, ShutsDownMidTransfer) {
  // The transfer should be in progress when shutting down.
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      FakeNearbyConnections::PayloadStatus::kInProgress, /*payload_id=*/1);
  nearby_process_manager_.fake_nearby_connections()
      .set_connection_established_listener(base::BindLambdaForTesting(
          [this]() { SendRts(/*file_payload_id=*/1); }));

  data_migration_.StartAdvertising();

  // Run until the transfers starts, then proceed to the test
  // harness's `TearDown()`.
  ASSERT_TRUE(base::test::RunUntil([this]() {
    return base::PathExists(BuildFilePayloadPath(/*payload_id=*/1));
  }));
}

}  // namespace
}  // namespace data_migration
