// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_sync_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/model/crypto/agile_symmetric_key.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/proto/tab_context_container_access_token.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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

  // Retrieves a copy of the issued `AgileSymmetricKeySet` associated with
  // `name`, or nullptr if no key set was issued for `name`.
  std::unique_ptr<syncer::AgileSymmetricKeySet> GetIssuedKeySet(
      const std::string& name) const {
    auto it = issued_key_sets_.find(name);
    if (it == issued_key_sets_.end()) {
      return nullptr;
    }
    return syncer::AgileSymmetricKeySet::FromProto(it->second);
  }

 private:
  bool should_fail_ = false;
  uint64_t next_server_token_id_ = 0;
  std::map<std::string, sync_pb::AgileSymmetricKeySet> issued_key_sets_;
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

  TabContextContainerAccessToken token_proto;
  ASSERT_TRUE(token_proto.ParseFromString(*token_string));
  EXPECT_FALSE(token_proto.name().empty());
  EXPECT_TRUE(token_proto.has_expire_time());
  EXPECT_EQ(token_proto.expire_time().seconds(), 1234567890);

  // Verify container key can be decrypted with the issued ephemeral key set.
  std::unique_ptr<syncer::AgileSymmetricKeySet> ephemeral_key_set =
      fake_fetcher_->GetIssuedKeySet(token_proto.name());
  ASSERT_THAT(ephemeral_key_set, NotNull());

  sync_pb::EncryptedData encrypted_data;
  ASSERT_TRUE(
      encrypted_data.ParseFromString(token_proto.encrypted_container_key()));

  std::optional<std::vector<uint8_t>> decrypted_bytes =
      ephemeral_key_set->Decrypt(encrypted_data);
  ASSERT_TRUE(decrypted_bytes.has_value());

  sync_pb::AgileSymmetricKeySet decrypted_key_set_proto;
  ASSERT_TRUE(decrypted_key_set_proto.ParseFromArray(decrypted_bytes->data(),
                                                     decrypted_bytes->size()));
  EXPECT_GT(decrypted_key_set_proto.key_size(), 0);
}

}  // namespace
}  // namespace sync_tab_context
