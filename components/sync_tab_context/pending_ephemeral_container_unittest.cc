// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/pending_ephemeral_container.h"

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/tab_context_container_access_token.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/tab_context_item_sync_bridge.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_tab_context {
namespace {

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Return;

class ManualEphemeralKeyFetcher : public EphemeralKeyFetcher {
 public:
  ManualEphemeralKeyFetcher() = default;
  ~ManualEphemeralKeyFetcher() override = default;

  void FetchEphemeralKey(FetchCallback callback) override {
    pending_callbacks_.push(std::move(callback));
  }

  size_t NumPendingFetches() const { return pending_callbacks_.size(); }

  void RespondWithSuccess(const std::string& name = "test_ephemeral_key") {
    ASSERT_FALSE(pending_callbacks_.empty());
    FetchCallback callback = std::move(pending_callbacks_.front());
    pending_callbacks_.pop();

    std::unique_ptr<syncer::AgileSymmetricKeySet> ephemeral_key =
        syncer::AgileSymmetricKeySet::CreateEmpty();
    ephemeral_key->RotatePrimaryToNewlyGeneratedRandomKey();

    EphemeralKeyFetcher::Result result{
        .ephemeral_key = std::move(ephemeral_key),
        .name = name,
        .expire_time = base::Time::FromSecondsSinceUnixEpoch(1234567890)};
    std::move(callback).Run(std::move(result));
  }

  void RespondWithFailure() {
    ASSERT_FALSE(pending_callbacks_.empty());
    FetchCallback callback = std::move(pending_callbacks_.front());
    pending_callbacks_.pop();
    std::move(callback).Run(std::nullopt);
  }

 private:
  std::queue<FetchCallback> pending_callbacks_;
};

class PendingEphemeralContainerTest : public ::testing::Test {
 protected:
  PendingEphemeralContainerTest() {
    ON_CALL(mock_item_processor_, IsTrackingMetadata)
        .WillByDefault(Return(true));
    item_bridge_ = std::make_unique<TabContextItemSyncBridge>(
        mock_item_processor_.CreateForwardingProcessor());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ManualEphemeralKeyFetcher fetcher_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>
      mock_item_processor_;
  std::unique_ptr<TabContextItemSyncBridge> item_bridge_;
};

TEST_F(PendingEphemeralContainerTest, ShouldCompleteOnFetchSuccess) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  base::test::TestFuture<std::optional<std::string>> completion_future;

  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, &fetcher_, item_bridge_.get());
  pending_container->Start(completion_future.GetCallback());

  ASSERT_EQ(fetcher_.NumPendingFetches(), 1u);
  const uint32_t expected_primary_key_id =
      pending_container->GetEncryptionKey().primary_key_id();
  ASSERT_NE(expected_primary_key_id, 0u);

  fetcher_.RespondWithSuccess("ephemeral_key_1");

  ASSERT_TRUE(completion_future.IsReady());
  const std::optional<std::string>& completed_access_token =
      completion_future.Get();
  ASSERT_TRUE(completed_access_token.has_value());

  std::unique_ptr<syncer::AgileSymmetricKeySet> taken_key_set =
      PendingEphemeralContainer::DestroyAndTakeKey(
          std::move(pending_container));
  ASSERT_THAT(taken_key_set, NotNull());
  EXPECT_EQ(taken_key_set->primary_key_id(), expected_primary_key_id);

  sync_pb::TabContextContainerAccessToken token_proto;
  ASSERT_TRUE(token_proto.ParseFromString(*completed_access_token));
  EXPECT_EQ(token_proto.name(), "ephemeral_key_1");
}

TEST_F(PendingEphemeralContainerTest,
       ShouldFlushQueuedAccessTokenCallbacksOnFetchSuccess) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  base::MockCallback<PendingEphemeralContainer::CompletionCallback>
      completion_callback;
  base::MockCallback<PendingEphemeralContainer::AccessTokenCallback>
      token_callback;

  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, &fetcher_, item_bridge_.get());
  pending_container->Start(completion_callback.Get());
  pending_container->QueueGetContainerAccessToken(token_callback.Get());

  {
    InSequence s;
    EXPECT_CALL(completion_callback, Run(Optional(Not(IsEmpty()))));
    EXPECT_CALL(token_callback, Run(Optional(Not(IsEmpty()))));
  }

  fetcher_.RespondWithSuccess("ephemeral_key_1");
}

TEST_F(PendingEphemeralContainerTest,
       ShouldFlushQueuedItemUploadsOnFetchSuccess) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  base::MockCallback<PendingEphemeralContainer::CompletionCallback>
      completion_callback;

  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, &fetcher_, item_bridge_.get());
  pending_container->Start(completion_callback.Get());

  const std::optional<sync_pb::EncryptedData> encrypted_content =
      pending_container->GetEncryptionKey().Encrypt(
          base::as_byte_span(std::string_view("payload")));
  ASSERT_TRUE(encrypted_content.has_value());

  base::test::TestFuture<UploadOutcome> upload_future;
  pending_container->QueueUploadItem("entry_1", *encrypted_content,
                                     upload_future.GetCallback());

  std::string committed_storage_key;
  {
    InSequence s;
    EXPECT_CALL(completion_callback, Run(Optional(Not(IsEmpty()))));
    EXPECT_CALL(mock_item_processor_, Put)
        .WillOnce([&committed_storage_key](
                      const std::string& storage_key,
                      std::unique_ptr<syncer::EntityData> entity_data,
                      syncer::MetadataChangeList*) {
          committed_storage_key = storage_key;
        });
  }

  fetcher_.RespondWithSuccess("ephemeral_key_1");

  // The queued upload was forwarded to `item_bridge_`.
  ASSERT_FALSE(committed_storage_key.empty());
  EXPECT_FALSE(upload_future.IsReady());

  syncer::EntityChangeList ack_changes;
  ack_changes.push_back(syncer::EntityChange::CreateDelete(
      committed_storage_key, syncer::EntityData()));
  item_bridge_->ApplyIncrementalSyncChanges(
      item_bridge_->CreateMetadataChangeList(), std::move(ack_changes));
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kSucceeded);
}

TEST_F(PendingEphemeralContainerTest,
       ShouldAllowDestroyingSelfInCompletionCallback) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, &fetcher_, item_bridge_.get());

  bool completion_called = false;
  pending_container->Start(
      base::BindLambdaForTesting([&completion_called, &pending_container](
                                     std::optional<std::string> access_token) {
        completion_called = true;
        EXPECT_TRUE(access_token.has_value());
        // Destroy `pending_container` synchronously inside the callback.
        pending_container.reset();
      }));

  base::test::TestFuture<std::optional<std::string>> token_future;
  pending_container->QueueGetContainerAccessToken(token_future.GetCallback());

  fetcher_.RespondWithSuccess("ephemeral_key_self_delete");

  EXPECT_TRUE(completion_called);
  EXPECT_THAT(pending_container, IsNull());
  ASSERT_TRUE(token_future.IsReady());
  EXPECT_TRUE(token_future.Get().has_value());
}

TEST_F(PendingEphemeralContainerTest,
       ShouldRetryWithExponentialBackoffAndSucceed) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  base::test::TestFuture<std::optional<std::string>> completion_future;

  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, &fetcher_, item_bridge_.get());
  pending_container->Start(completion_future.GetCallback());

  // Fail the first 4 attempts; each failure schedules a retry.
  for (int attempt = 1; attempt <= 4; ++attempt) {
    ASSERT_EQ(fetcher_.NumPendingFetches(), 1u);
    fetcher_.RespondWithFailure();
    EXPECT_FALSE(completion_future.IsReady());
    task_environment_.FastForwardBy(base::Seconds(30));
  }

  // The 5th attempt succeeds.
  ASSERT_EQ(fetcher_.NumPendingFetches(), 1u);
  fetcher_.RespondWithSuccess("ephemeral_key_after_retries");

  ASSERT_TRUE(completion_future.IsReady());
  EXPECT_TRUE(completion_future.Get().has_value());
}

TEST_F(PendingEphemeralContainerTest,
       ShouldFailAfterMaxRetriesAndNotifyQueuedCallbacks) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  base::MockCallback<PendingEphemeralContainer::CompletionCallback>
      completion_callback;
  base::MockCallback<PendingEphemeralContainer::UploadCallback> upload_callback;
  base::MockCallback<PendingEphemeralContainer::AccessTokenCallback>
      token_callback;

  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, &fetcher_, item_bridge_.get());
  pending_container->Start(completion_callback.Get());
  pending_container->QueueUploadItem("entry_1", sync_pb::EncryptedData(),
                                     upload_callback.Get());
  pending_container->QueueGetContainerAccessToken(token_callback.Get());

  for (int attempt = 1; attempt <= 4; ++attempt) {
    ASSERT_EQ(fetcher_.NumPendingFetches(), 1u);
    fetcher_.RespondWithFailure();
    task_environment_.FastForwardBy(base::Seconds(30));
  }

  {
    InSequence s;
    EXPECT_CALL(completion_callback, Run(Eq(std::nullopt)));
    EXPECT_CALL(upload_callback, Run(UploadOutcome::kFailed));
    EXPECT_CALL(token_callback, Run(Eq(std::nullopt)));
  }

  // Fail the 5th (final) attempt.
  ASSERT_EQ(fetcher_.NumPendingFetches(), 1u);
  fetcher_.RespondWithFailure();
}

TEST_F(PendingEphemeralContainerTest,
       ShouldFailQueuedCallbacksWhenDestroyedBeforeCompletion) {
  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  base::test::TestFuture<std::optional<std::string>> completion_future;

  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, &fetcher_, item_bridge_.get());
  pending_container->Start(completion_future.GetCallback());

  base::test::TestFuture<UploadOutcome> upload_future;
  pending_container->QueueUploadItem("entry_1", sync_pb::EncryptedData(),
                                     upload_future.GetCallback());

  base::test::TestFuture<std::optional<std::string>> token_future;
  pending_container->QueueGetContainerAccessToken(token_future.GetCallback());

  pending_container.reset();

  EXPECT_FALSE(completion_future.IsReady());
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kFailed);
  EXPECT_EQ(token_future.Get(), std::nullopt);
}

}  // namespace
}  // namespace sync_tab_context
