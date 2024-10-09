// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/device.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
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

class DeviceTest : public ::testing::Test {
 public:
  DeviceTest()
      : nearby_process_manager_(kRemoteEndpointId),
        nearby_connections_manager_(&nearby_process_manager_, kServiceId) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    home_dir_override_.emplace(base::DIR_HOME, temp_dir_.GetPath());
    ConnectionBarrier connection_barrier(&nearby_connections_manager_);
    NearbyConnection* nearby_connection = connection_barrier.Wait();
    ASSERT_TRUE(nearby_connection);
    ASSERT_TRUE(base::CreateDirectory(GetFilePayloadDirectory()));
    nearby_process_manager_.fake_nearby_connections()
        .set_local_to_remote_payload_listener(base::BindRepeating(
            &DeviceTest::RespondToCtsMessage, base::Unretained(this)));
    device_.emplace(nearby_connection, &nearby_connections_manager_);
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
    // Must not intersect with file payload ids, which are assigned starting at
    // "1" in test cases below.
    static int64_t g_rts_payload_id_assigner = 1000000;
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
  NearbyConnectionsManagerImpl nearby_connections_manager_;
  std::optional<Device> device_;
};

TEST_F(DeviceTest, TransmitsAllFiles) {
  // Remote device send RTS for some files.
  SendRts(/*file_payload_id=*/1);
  SendRts(/*file_payload_id=*/2);
  SendRts(/*file_payload_id=*/3);

  ASSERT_TRUE(base::test::RunUntil([this]() {
    // All expected files have been written to disc.
    return base::ranges::all_of(
        requested_file_payload_ids_,
        [this](int64_t payload_id) { return FileIsReady(payload_id); });
  }));
  // For extra safety, flush any pending tasks to ensure the `Device` is
  // completely idle before checking the results.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/1)),
            expected_file_content_.at(/*payload_id=*/1));
  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/2)),
            expected_file_content_.at(/*payload_id=*/2));
  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/3)),
            expected_file_content_.at(/*payload_id=*/3));
}

// Verifies that the Device can be destroyed gracefully mid-transfer. Does not
// have any real expectation other than the code doesn't crash.
TEST_F(DeviceTest, ShutsDownMidTransfer) {
  constexpr int64_t kTotalNumFilesToTransmit = 5;
  // Remote device send RTS for some files.
  for (int64_t file_payload_id = 1; file_payload_id <= kTotalNumFilesToTransmit;
       ++file_payload_id) {
    nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
        ::nearby::connections::mojom::PayloadStatus::kInProgress,
        file_payload_id);
    SendRts(file_payload_id);
  }

  ASSERT_TRUE(base::test::RunUntil([this]() {
    // Any file has been written to disc (exit any time mid-transfer).
    for (auto payload_id : requested_file_payload_ids_) {
      if (base::PathExists(BuildFilePayloadPath(payload_id))) {
        return true;
      }
    }
    return false;
  }));

  ASSERT_LT(base::ComputeDirectorySize(temp_dir_.GetPath()),
            kTotalNumFilesToTransmit *
                nearby_process_manager_.fake_nearby_connections()
                    .test_file_size_in_bytes());
  device_.reset();
}

TEST_F(DeviceTest, ContinuesTransmittingFilesAfterFailure) {
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      ::nearby::connections::mojom::PayloadStatus::kFailure, /*payload_id=*/2);

  // Remote device send RTS for some files.
  SendRts(/*file_payload_id=*/1);
  SendRts(/*file_payload_id=*/2);
  SendRts(/*file_payload_id=*/3);

  ASSERT_TRUE(base::test::RunUntil([this]() {
    return FileIsReady(/*payload_id=*/1) && FileIsReady(/*payload_id=*/3);
  }));
  // For extra safety, flush any pending tasks to ensure the `Device` is
  // completely idle before checking the results.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/1)),
            expected_file_content_.at(/*payload_id=*/1));
  EXPECT_EQ(base::ReadFileToBytes(BuildFilePayloadPath(/*payload_id=*/3)),
            expected_file_content_.at(/*payload_id=*/3));
}

TEST_F(DeviceTest, FileStressTest) {
  constexpr int64_t kTotalNumFilesToTransmit = 5000;
  // Reduce these numbers from their defaults otherwise the test takes 3-4x
  // longer.
  constexpr int64_t kFileSizeInBytes = 1;
  constexpr int64_t kNumChunksPerFile = 1;

  // Empirically, this test takes about 15 seconds.
  base::test::ScopedRunLoopTimeout test_timeout(FROM_HERE, base::Minutes(1));

  nearby_process_manager_.fake_nearby_connections().set_test_file_size_in_bytes(
      kFileSizeInBytes);
  nearby_process_manager_.fake_nearby_connections().set_test_file_num_chunks(
      kNumChunksPerFile);

  // Remote device send RTS for some files.
  for (int64_t payload_id = 1; payload_id <= kTotalNumFilesToTransmit;
       ++payload_id) {
    SendRts(payload_id);
  }

  base::RepeatingClosure test_complete_signal = task_environment_.QuitClosure();
  // The `test_completion_check` is expensive, so it's run on a blocking thread,
  // freeing the main thread for the test itself.
  base::RepeatingClosure test_completion_check =
      base::BindLambdaForTesting([this, test_complete_signal]() {
        if (base::ComputeDirectorySize(GetFilePayloadDirectory()) >=
            kTotalNumFilesToTransmit * kFileSizeInBytes) {
          test_complete_signal.Run();
        }
      });
  scoped_refptr<base::SequencedTaskRunner> test_completion_sequence =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  auto test_completion_checker = std::make_unique<base::RepeatingTimer>();
  test_completion_sequence->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&test_completion_checker,
                                             test_completion_check]() {
        constexpr base::TimeDelta kTestCompletionCheckInterval =
            base::Seconds(1);
        test_completion_checker->Start(FROM_HERE, kTestCompletionCheckInterval,
                                       test_completion_check);
      }));

  task_environment_.RunUntilQuit();
  test_completion_sequence->DeleteSoon(FROM_HERE,
                                       std::move(test_completion_checker));
  // Flush any pending ThreadPool tasks scheduled internally by `DataMigration`
  // before exiting.
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace data_migration
