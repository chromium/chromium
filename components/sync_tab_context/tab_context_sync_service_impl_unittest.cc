// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_sync_service_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/crypto/agile_symmetric_key.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/tab_context_container_access_token.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_worker.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace sync_tab_context {
namespace {

using ::testing::NotNull;

// Fake implementation of `EphemeralKeyFetcher` for testing. Generates a random
// `AgileSymmetricKeySet` and an auto-incremented server token ID for each
// fetch, storing the generated key set proto in a map for test verification. As
// opposed to the real implementation, this class implements synchronous
// behavior, meaning that the completion callbacks are triggered immediately,
// before returning from the function.
class FakeEphemeralKeyFetcher : public EphemeralKeyFetcher {
 public:
  FakeEphemeralKeyFetcher() = default;

  void FetchEphemeralKey(FetchCallback callback) override {
    if (should_fail_) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    const std::string name = base::NumberToString(++next_server_token_id_);
    auto key_set = syncer::AgileSymmetricKeySet::CreateEmpty();
    key_set->RotatePrimaryToNewlyGeneratedRandomKey();
    issued_key_sets_[name] = key_set->ToProto();

    EphemeralKeyFetcher::Result result{
        .ephemeral_key = std::move(key_set),
        .name = name,
        .expire_time = base::Time::FromSecondsSinceUnixEpoch(1234567890)};
    std::move(callback).Run(std::move(result));
  }

  // Configures whether `FetchEphemeralKey` should simulate a fetch failure.
  void set_should_fail(bool fail) { should_fail_ = fail; }

  // Decrypts the container key set from `serialized_access_token` using the
  // previously issued ephemeral key set corresponding to the token's name.
  // Returns nullptr on failure.
  std::unique_ptr<syncer::AgileSymmetricKeySet> DecryptContainerKeySet(
      std::string_view serialized_access_token) const {
    sync_pb::TabContextContainerAccessToken token_proto;
    if (!token_proto.ParseFromString(serialized_access_token)) {
      return nullptr;
    }
    const sync_pb::AgileSymmetricKeySet* issued_key_set =
        base::FindOrNull(issued_key_sets_, token_proto.name());
    if (!issued_key_set) {
      return nullptr;
    }
    std::unique_ptr<syncer::AgileSymmetricKeySet> ephemeral_key_set =
        syncer::AgileSymmetricKeySet::FromProto(*issued_key_set);
    if (!ephemeral_key_set) {
      return nullptr;
    }
    sync_pb::EncryptedData encrypted_container_key;
    if (!encrypted_container_key.ParseFromString(
            token_proto.encrypted_container_key())) {
      return nullptr;
    }
    std::optional<std::vector<uint8_t>> decrypted_key_set_bytes =
        ephemeral_key_set->Decrypt(encrypted_container_key);
    if (!decrypted_key_set_bytes.has_value()) {
      return nullptr;
    }
    sync_pb::AgileSymmetricKeySet container_key_set_proto;
    if (!container_key_set_proto.ParseFromArray(
            decrypted_key_set_bytes->data(), decrypted_key_set_bytes->size())) {
      return nullptr;
    }
    return syncer::AgileSymmetricKeySet::FromProto(container_key_set_proto);
  }

  // Simulates server-side decryption: extracts the container key set from
  // `serialized_access_token` using the issued ephemeral key, and then decrypts
  // `encrypted_data` using that container key set.
  std::optional<std::vector<uint8_t>> DecryptWithAccessToken(
      std::string_view serialized_access_token,
      const sync_pb::EncryptedData& encrypted_data) const {
    std::unique_ptr<syncer::AgileSymmetricKeySet> container_key_set =
        DecryptContainerKeySet(serialized_access_token);
    if (!container_key_set) {
      return std::nullopt;
    }
    return container_key_set->Decrypt(encrypted_data);
  }

 private:
  bool should_fail_ = false;
  uint64_t next_server_token_id_ = 0;
  base::flat_map<std::string, sync_pb::AgileSymmetricKeySet> issued_key_sets_;
};

class TabContextSyncServiceImplTest : public ::testing::Test {
 protected:
  TabContextSyncServiceImplTest() {
    auto fetcher = std::make_unique<FakeEphemeralKeyFetcher>();
    fake_fetcher_ = fetcher.get();
    store_ = syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest();
    syncer::DataTypeStoreTestUtil::WriteInitialSyncDoneAndWait(*store_);
    service_ = std::make_unique<TabContextSyncServiceImpl>(
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        std::move(fetcher), base::DoNothing());

    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    request.cache_guid = "test_cache_guid";
    base::test::TestFuture<std::unique_ptr<syncer::DataTypeActivationResponse>>
        future;
    service_->GetSyncControllerDelegateForItem()->OnSyncStarting(
        request, future.GetCallback());
    item_worker_ =
        syncer::MockDataTypeWorker::CreateWorkerAndConnectSync(future.Take());
  }

  void TearDown() override {
    // Null out `fake_fetcher_` before `service_` is destroyed and frees the
    // `FakeEphemeralKeyFetcher` instance, preventing PartitionAlloc dangling
    // pointer warnings.
    fake_fetcher_ = nullptr;
  }

  // Helper method to synchronously call `GetContainerAccessToken()` on
  // `service_`.
  std::optional<std::string> GetContainerAccessToken(
      const ContainerId& container_id) {
    base::test::TestFuture<std::optional<std::string>> future;
    service_->GetContainerAccessToken(container_id, future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  raw_ptr<FakeEphemeralKeyFetcher> fake_fetcher_;
  std::unique_ptr<TabContextSyncServiceImpl> service_;
  std::unique_ptr<syncer::MockDataTypeWorker> item_worker_;
};

TEST_F(TabContextSyncServiceImplTest,
       ShouldReturnNulloptWhenContainerNotFound) {
  EXPECT_EQ(
      GetContainerAccessToken(ContainerId(base::Uuid::GenerateRandomV4())),
      std::nullopt);
}

TEST_F(TabContextSyncServiceImplTest, ShouldReturnNulloptWhenFetcherFails) {
  fake_fetcher_->set_should_fail(true);

  // Wait for the store to finish loading so ModelReadyToSync is called.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsActiveForTesting(); }));

  std::optional<ContainerId> container_id = service_->CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  EXPECT_EQ(GetContainerAccessToken(*container_id), std::nullopt);
}

TEST_F(TabContextSyncServiceImplTest,
       ShouldReturnAccessTokenWhenContainerExists) {
  // Wait for the store to finish loading so ModelReadyToSync is called.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsActiveForTesting(); }));

  std::optional<ContainerId> container_id = service_->CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  std::optional<std::string> token_string =
      GetContainerAccessToken(*container_id);
  ASSERT_TRUE(token_string.has_value());

  sync_pb::TabContextContainerAccessToken token_proto;
  ASSERT_TRUE(token_proto.ParseFromString(*token_string));
  EXPECT_FALSE(token_proto.name().empty());
  EXPECT_TRUE(token_proto.has_expire_time());
  EXPECT_EQ(token_proto.expire_time().seconds(), 1234567890);

  // Verify container key set can be decrypted from the access token.
  EXPECT_THAT(fake_fetcher_->DecryptContainerKeySet(*token_string), NotNull());
}

TEST_F(TabContextSyncServiceImplTest,
       ShouldCompressAndEncryptPageContextOnUpload) {
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsActiveForTesting(); }));

  std::optional<ContainerId> container_id = service_->CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  const std::string kPageContext =
      "Repeated text to test upload. Repeated text to test upload.";
  base::test::TestFuture<UploadOutcome> upload_future;
  service_->UploadPageContext(*container_id, "entry_1", kPageContext,
                              upload_future.GetCallback());

  const std::vector<const syncer::CommitRequestData*> commit_batch =
      item_worker_->WaitForPendingCommits();
  ASSERT_EQ(commit_batch.size(), 1u);
  EXPECT_FALSE(upload_future.IsReady());

  ASSERT_THAT(commit_batch[0]->entity, NotNull());

  const sync_pb::EncryptedTabContextItemSpecifics& item_specifics =
      commit_batch[0]->entity->specifics.encrypted_tab_context_item();
  EXPECT_EQ(item_specifics.container_id(),
            container_id->value().AsLowercaseString());
  EXPECT_EQ(item_specifics.item_id(), "entry_1");
  ASSERT_TRUE(item_specifics.has_encrypted_content());

  // Decrypt the uploaded payload using the container access token.
  std::optional<std::string> token_string =
      GetContainerAccessToken(*container_id);
  ASSERT_TRUE(token_string.has_value());

  std::optional<std::vector<uint8_t>> decrypted_bytes =
      fake_fetcher_->DecryptWithAccessToken(*token_string,
                                            item_specifics.encrypted_content());
  ASSERT_TRUE(decrypted_bytes.has_value());

  sync_pb::TabContextItemContent item_content;
  ASSERT_TRUE(item_content.ParseFromArray(decrypted_bytes->data(),
                                          decrypted_bytes->size()));
  EXPECT_FALSE(item_content.has_raw_data());
  ASSERT_TRUE(item_content.has_gzip_compressed_data());

  std::string uncompressed_page_context;
  ASSERT_TRUE(compression::GzipUncompress(item_content.gzip_compressed_data(),
                                          &uncompressed_page_context));
  EXPECT_EQ(uncompressed_page_context, kPageContext);

  item_worker_->AckOnePendingCommit();
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kSucceeded);
}

TEST_F(TabContextSyncServiceImplTest,
       ShouldFailUploadPageContextWhenContainerDoesNotExist) {
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsActiveForTesting(); }));

  base::test::TestFuture<UploadOutcome> upload_future;
  service_->UploadPageContext(ContainerId(base::Uuid::GenerateRandomV4()),
                              "entry_1", "page_context",
                              upload_future.GetCallback());
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kFailed);
  EXPECT_EQ(item_worker_->GetNumPendingCommits(), 0u);
}

TEST_F(TabContextSyncServiceImplTest,
       ShouldRetryUploadPageContextAfterTransientCommitError) {
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsActiveForTesting(); }));

  std::optional<ContainerId> container_id = service_->CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  base::test::TestFuture<UploadOutcome> upload_future;
  service_->UploadPageContext(*container_id, "entry_1", "page_context",
                              upload_future.GetCallback());
  ASSERT_EQ(item_worker_->WaitForPendingCommits().size(), 1u);

  // Simulate a transient per-item commit error. The callback should remain
  // pending and the processor should be able to reload the item via
  // GetDataForCommit() on the next sync cycle.
  item_worker_->FailOneCommit();
  EXPECT_FALSE(upload_future.IsReady());

  item_worker_->NudgeForCommit();
  ASSERT_EQ(item_worker_->GetNumPendingCommits(), 1u);

  item_worker_->AckOnePendingCommit();
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kSucceeded);
}

TEST_F(TabContextSyncServiceImplTest,
       ShouldFailUploadPageContextWhenCommitFails) {
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsActiveForTesting(); }));

  std::optional<ContainerId> container_id = service_->CreateContainer();
  ASSERT_TRUE(container_id.has_value());

  base::test::TestFuture<UploadOutcome> upload_future;
  service_->UploadPageContext(*container_id, "entry_1", "page_context",
                              upload_future.GetCallback());
  ASSERT_EQ(item_worker_->WaitForPendingCommits().size(), 1u);

  item_worker_->FailFullCommitRequest();
  EXPECT_EQ(upload_future.Get(), UploadOutcome::kFailed);
}

}  // namespace
}  // namespace sync_tab_context
