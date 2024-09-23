// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/file_transfer.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/components/data_migration/pending_file_transfer_queue.h"
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

class FileTransferTest : public ::testing::Test {
 protected:
  FileTransferTest()
      : nearby_process_manager_(kRemoteEndpointId),
        nearby_connections_manager_(&nearby_process_manager_, kServiceId) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    home_dir_override_.emplace(base::DIR_HOME, temp_dir_.GetPath());
    ConnectionBarrier connection_barrier(&nearby_connections_manager_);
    nearby_connection_ = connection_barrier.Wait();
    ASSERT_TRUE(nearby_connection_);
    ASSERT_TRUE(base::CreateDirectory(GetFilePayloadDirectory()));
  }

  base::FilePath GetFilePayloadDirectory() {
    return temp_dir_.GetPath().Append(kPayloadTargetDir);
  }

  base::FilePath BuildFilePayloadName(int64_t payload_id) {
    return base::FilePath(base::StringPrintf("payload_%ld", payload_id));
  }

  void InitializeFileTransfer() {
    completion_future_.Clear();
    file_transfer_.emplace(
        nearby_connection_.get(), &nearby_connections_manager_,
        pending_file_transfer_queue_, completion_future_.GetCallback());
  }

  void VerifyCTSMessage(::nearby::connections::mojom::PayloadPtr cts_payload,
                        int64_t expected_file_payload_id) {
    ASSERT_TRUE(cts_payload);
    ASSERT_TRUE(cts_payload->content->is_bytes());
    const std::vector<uint8_t>& cts_bytes =
        cts_payload->content->get_bytes()->bytes;
    ASSERT_EQ(std::string(cts_bytes.begin(), cts_bytes.end()),
              base::NumberToString(expected_file_payload_id));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::optional<base::ScopedPathOverride> home_dir_override_;
  FakeNearbyProcessManager nearby_process_manager_;
  NearbyConnectionsManagerImpl nearby_connections_manager_;
  raw_ptr<NearbyConnection> nearby_connection_;
  PendingFileTransferQueue pending_file_transfer_queue_;
  std::optional<FileTransfer> file_transfer_;
  base::test::TestFuture<bool> completion_future_;
};

TEST_F(FileTransferTest, SuccessfulTransfers) {
  auto run_transfer_file_test = [this](int64_t file_payload_id) {
    InitializeFileTransfer();
    pending_file_transfer_queue_.Push(file_payload_id);
    std::vector<uint8_t> expected_file_bytes;
    nearby_process_manager_.fake_nearby_connections()
        .set_local_to_remote_payload_listener(base::BindLambdaForTesting(
            [this, &expected_file_bytes, file_payload_id](
                ::nearby::connections::mojom::PayloadPtr cts_payload) {
              // Once CTS is received on the remote device, start transferring
              // the file.
              VerifyCTSMessage(std::move(cts_payload), file_payload_id);
              ASSERT_TRUE(
                  nearby_process_manager_.fake_nearby_connections().SendFile(
                      file_payload_id, &expected_file_bytes));
            }));
    ASSERT_TRUE(completion_future_.Get<0>());
    EXPECT_EQ(base::ReadFileToBytes(GetFilePayloadDirectory().Append(
                  BuildFilePayloadName(file_payload_id))),
              expected_file_bytes);
  };

  run_transfer_file_test(1);
  run_transfer_file_test(2);
}

TEST_F(FileTransferTest, FailedTransfer) {
  constexpr int64_t kFilePayloadId = 1;
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      ::nearby::connections::mojom::PayloadStatus::kFailure, /*payload_id=*/1);
  InitializeFileTransfer();
  pending_file_transfer_queue_.Push(kFilePayloadId);
  nearby_process_manager_.fake_nearby_connections()
      .set_local_to_remote_payload_listener(base::BindLambdaForTesting(
          [this](::nearby::connections::mojom::PayloadPtr cts_payload) {
            // Once CTS is received on the remote device, start transferring
            // the file.
            VerifyCTSMessage(std::move(cts_payload), kFilePayloadId);
            ASSERT_TRUE(
                nearby_process_manager_.fake_nearby_connections().SendFile(
                    kFilePayloadId));
          }));
  EXPECT_FALSE(completion_future_.Get<0>());
}

}  // namespace
}  // namespace data_migration
