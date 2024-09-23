// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/file_receiver.h"

#include <cstdint>
#include <optional>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/components/data_migration/testing/connection_barrier.h"
#include "chromeos/ash/components/data_migration/testing/fake_nearby_connections.h"
#include "chromeos/ash/components/data_migration/testing/fake_nearby_process_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_migration {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;

constexpr char kRemoteEndpointId[] = "test-remote-endpoint";

class MockFileReceiverObserver {
 public:
  MockFileReceiverObserver() = default;
  MockFileReceiverObserver(const MockFileReceiverObserver&) = delete;
  MockFileReceiverObserver& operator=(const MockFileReceiverObserver&) = delete;
  ~MockFileReceiverObserver() = default;

  void ExpectCompleteFileTransfer(
      bool success,
      int64_t payload_id,
      std::vector<uint8_t>* expected_file_content,
      FakeNearbyProcessManager* nearby_process_manager,
      base::OnceClosure on_file_transfer_complete_cb) {
    InSequence sequence;
    EXPECT_CALL(*this, OnFileRegistered())
        .WillOnce(Invoke(
            [payload_id, nearby_process_manager, expected_file_content]() {
              ASSERT_TRUE(
                  nearby_process_manager->fake_nearby_connections().SendFile(
                      payload_id, expected_file_content));
            }));
    EXPECT_CALL(*this, OnFileTransferComplete(success))
        .WillOnce(
            Invoke([cb = std::move(on_file_transfer_complete_cb)]() mutable {
              std::move(cb).Run();
            }));
  }

  FileReceiver::Observer CreateCallbacks() {
    return {base::BindOnce(&MockFileReceiverObserver::OnFileRegistered,
                           base::Unretained(this)),
            base::BindOnce(&MockFileReceiverObserver::OnFileTransferComplete,
                           base::Unretained(this))};
  }

  MOCK_METHOD(void, OnFileRegistered, ());
  MOCK_METHOD(void, OnFileTransferComplete, (bool));
};

class FileReceiverTest : public ::testing::Test {
 protected:
  FileReceiverTest()
      : nearby_process_manager_(kRemoteEndpointId),
        nearby_connections_manager_(&nearby_process_manager_, kServiceId) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_payload_path_ = temp_dir_.GetPath().Append("test_payload");
    ConnectionBarrier connection_barrier(&nearby_connections_manager_);
    ASSERT_TRUE(connection_barrier.Wait());
  }

  // Use `MOCK_TIME` to speed up
  // `NearbyConnections::Manager::RegisterPayloadPath()` retries on failure.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  FakeNearbyProcessManager nearby_process_manager_;
  NearbyConnectionsManagerImpl nearby_connections_manager_;
  MockFileReceiverObserver observer_;
  base::FilePath test_payload_path_;
};

TEST_F(FileReceiverTest, SingleFile) {
  base::test::TestFuture<void> completion_signal;
  std::vector<uint8_t> expected_file_content;
  observer_.ExpectCompleteFileTransfer(
      /*success=*/true, /*payload_id=*/1, &expected_file_content,
      &nearby_process_manager_, completion_signal.GetCallback());
  FileReceiver receiver(/*payload_id=*/1, test_payload_path_,
                        observer_.CreateCallbacks(),
                        &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal.Wait());
  EXPECT_EQ(base::ReadFileToBytes(test_payload_path_), expected_file_content);
}

TEST_F(FileReceiverTest, MultipleFiles) {
  MockFileReceiverObserver observer_2;
  base::FilePath payload_path_1 = temp_dir_.GetPath().Append("test_payload_1");
  base::FilePath payload_path_2 = temp_dir_.GetPath().Append("test_payload_2");
  base::test::TestFuture<void> completion_signal_1, completion_signal_2;
  std::vector<uint8_t> expected_file_content_1, expected_file_content_2;
  observer_.ExpectCompleteFileTransfer(
      /*success=*/true, /*payload_id=*/1, &expected_file_content_1,
      &nearby_process_manager_, completion_signal_1.GetCallback());
  observer_2.ExpectCompleteFileTransfer(
      /*success=*/true, /*payload_id=*/2, &expected_file_content_2,
      &nearby_process_manager_, completion_signal_2.GetCallback());
  FileReceiver receiver_1(/*payload_id=*/1, payload_path_1,
                          observer_.CreateCallbacks(),
                          &nearby_connections_manager_);
  FileReceiver receiver_2(/*payload_id=*/2, payload_path_2,
                          observer_2.CreateCallbacks(),
                          &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal_1.Wait());
  ASSERT_TRUE(completion_signal_2.Wait());
  EXPECT_EQ(base::ReadFileToBytes(payload_path_1), expected_file_content_1);
  EXPECT_EQ(base::ReadFileToBytes(payload_path_2), expected_file_content_2);
}

TEST_F(FileReceiverTest, FailedTransfer) {
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      ::nearby::connections::mojom::PayloadStatus::kFailure,
      /*payload_id=*/1);
  base::test::TestFuture<void> completion_signal;
  observer_.ExpectCompleteFileTransfer(
      /*success=*/false, /*payload_id=*/1, /*expected_file_content=*/nullptr,
      &nearby_process_manager_, completion_signal.GetCallback());
  FileReceiver receiver(
      /*payload_id=*/1, test_payload_path_, observer_.CreateCallbacks(),
      &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal.Wait());
}

TEST_F(FileReceiverTest, CancelledTransfer) {
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      ::nearby::connections::mojom::PayloadStatus::kCanceled, /*payload_id=*/1);
  base::test::TestFuture<void> completion_signal;
  observer_.ExpectCompleteFileTransfer(
      /*success=*/false, /*payload_id=*/1, /*expected_file_content=*/nullptr,
      &nearby_process_manager_, completion_signal.GetCallback());
  FileReceiver receiver(
      /*payload_id=*/1, test_payload_path_, observer_.CreateCallbacks(),
      &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal.Wait());
}

TEST_F(FileReceiverTest, FileReceiverDestroyedWhileInProgress) {
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      ::nearby::connections::mojom::PayloadStatus::kInProgress,
      /*payload_id=*/1);
  base::test::TestFuture<void> completion_signal;
  EXPECT_CALL(observer_, OnFileRegistered())
      .WillOnce(Invoke([this, &completion_signal]() {
        ASSERT_TRUE(nearby_process_manager_.fake_nearby_connections().SendFile(
            /*payload_id=*/1, /*expected_file_content=*/nullptr));
        // The file should be partially filled at this point.
        ASSERT_TRUE(base::PathExists(test_payload_path_));
        completion_signal.GetCallback().Run();
      }));
  EXPECT_CALL(observer_, OnFileTransferComplete(_)).Times(0);

  auto receiver = std::make_optional<FileReceiver>(
      /*payload_id=*/1, test_payload_path_, observer_.CreateCallbacks(),
      &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal.Wait());
  receiver.reset();
}

// The test's intention is to make sure there are no crashes when the
// `FileReceiver` is destroyed synchronously within the completion callback.
// This is expected to be a common use case. There is no specific test
// expectation in here other than "it runs without crashing".
TEST_F(FileReceiverTest, FileReceiverDestroyedWithinCompletionCallback) {
  base::test::TestFuture<void> completion_signal;
  std::optional<FileReceiver> receiver;
  ON_CALL(observer_, OnFileRegistered()).WillByDefault(Invoke([this]() {
    ASSERT_TRUE(nearby_process_manager_.fake_nearby_connections().SendFile(
        /*payload_id=*/1, /*expected_file_content=*/nullptr));
  }));
  ON_CALL(observer_, OnFileTransferComplete(/*success=*/true))
      .WillByDefault(Invoke([&receiver, &completion_signal]() {
        receiver.reset();
        completion_signal.GetCallback().Run();
      }));

  receiver.emplace(
      /*payload_id=*/1, test_payload_path_, observer_.CreateCallbacks(),
      &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal.Wait());
}

TEST_F(FileReceiverTest, FileRegistrationError) {
  // The first `NearbyConnections::Manager::RegisterPayloadPath()` should fail,
  // and `FileReceiver` should retry shortly after. The retry should succeed.
  bool is_first_registration = true;
  nearby_process_manager_.fake_nearby_connections()
      .set_register_payload_file_result_generator(
          base::BindLambdaForTesting([&is_first_registration]() {
            FakeNearbyConnections::Status result =
                is_first_registration ? FakeNearbyConnections::Status::kError
                                      : FakeNearbyConnections::Status::kSuccess;
            is_first_registration = false;
            return result;
          }));
  base::test::TestFuture<void> completion_signal;
  EXPECT_CALL(observer_, OnFileRegistered())
      .WillOnce(Invoke(
          [&completion_signal]() { completion_signal.GetCallback().Run(); }));
  FileReceiver receiver(/*payload_id=*/1, test_payload_path_,
                        observer_.CreateCallbacks(),
                        &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal.Wait());
}

TEST_F(FileReceiverTest, FileRegistrationPermanentError) {
  nearby_process_manager_.fake_nearby_connections()
      .set_register_payload_file_result_generator(base::BindLambdaForTesting(
          []() { return FakeNearbyConnections::Status::kError; }));
  base::test::TestFuture<void> completion_signal;
  EXPECT_CALL(observer_, OnFileRegistered()).Times(0);
  EXPECT_CALL(observer_, OnFileTransferComplete(false))
      .WillOnce(Invoke(
          [&completion_signal]() { completion_signal.GetCallback().Run(); }));
  FileReceiver receiver(/*payload_id=*/1, test_payload_path_,
                        observer_.CreateCallbacks(),
                        &nearby_connections_manager_);
  ASSERT_TRUE(completion_signal.Wait());
}

TEST_F(FileReceiverTest, OneFileTransferOverridesAnother) {
  // First file transfer is still in progress when it gets canceled.
  nearby_process_manager_.fake_nearby_connections().SetFinalFilePayloadStatus(
      ::nearby::connections::mojom::PayloadStatus::kInProgress,
      /*payload_id=*/1);
  base::test::TestFuture<void> transfer_started_signal;
  EXPECT_CALL(observer_, OnFileRegistered())
      .WillOnce(Invoke([this, &transfer_started_signal]() {
        ASSERT_TRUE(nearby_process_manager_.fake_nearby_connections().SendFile(
            /*payload_id=*/1, /*expected_file_content=*/nullptr));
        ASSERT_TRUE(base::PathExists(test_payload_path_));
        transfer_started_signal.GetCallback().Run();
      }));
  auto receiver = std::make_optional<FileReceiver>(
      /*payload_id=*/1, test_payload_path_, observer_.CreateCallbacks(),
      &nearby_connections_manager_);
  ASSERT_TRUE(transfer_started_signal.Wait());

  // Start second file transfer immediately after. This one should succeed.
  // Note it intentionally uses a different payload id, but writes to the same
  // path.
  transfer_started_signal.Clear();
  receiver.reset();
  Mock::VerifyAndClearExpectations(&observer_);

  InSequence sequence;
  std::vector<uint8_t> expected_file_content;
  EXPECT_CALL(observer_, OnFileRegistered())
      .WillOnce(Invoke([this, &expected_file_content]() {
        ASSERT_TRUE(nearby_process_manager_.fake_nearby_connections().SendFile(
            /*payload_id=*/2, &expected_file_content));
      }));
  base::test::TestFuture<void> transfer_complete_signal;
  EXPECT_CALL(observer_, OnFileTransferComplete(/*success=*/true))
      .WillOnce(Invoke([&transfer_complete_signal]() {
        transfer_complete_signal.GetCallback().Run();
      }));
  receiver.emplace(
      /*payload_id=*/2, test_payload_path_, observer_.CreateCallbacks(),
      &nearby_connections_manager_);
  ASSERT_TRUE(transfer_complete_signal.Wait());
  EXPECT_EQ(base::ReadFileToBytes(test_payload_path_), expected_file_content);
}

}  // namespace
}  // namespace data_migration
