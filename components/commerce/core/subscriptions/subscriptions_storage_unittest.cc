// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

const int64_t kMockTimestamp1 = 123456;
const int64_t kMockTimestamp2 = 234567;
const int64_t kMockTimestamp3 = 345678;
const int64_t kMockTimestamp4 = 456789;
const std::string kMockId1 = "111";
const std::string kMockId2 = "222";
const std::string kMockId3 = "333";
const std::string kKey1 = "PRICE_TRACK_PRODUCT_CLUSTER_ID_111";
const std::string kKey2 = "PRICE_TRACK_PRODUCT_CLUSTER_ID_222";
const std::string kKey3 = "PRICE_TRACK_PRODUCT_CLUSTER_ID_333";

std::unique_ptr<std::vector<commerce::CommerceSubscription>>
MockIncomingSubscriptions() {
  auto subscriptions =
      std::make_unique<std::vector<commerce::CommerceSubscription>>();
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId1,
      commerce::ManagementType::kUserManaged));
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId2,
      commerce::ManagementType::kUserManaged));
  return subscriptions;
}

std::unique_ptr<std::vector<commerce::CommerceSubscription>>
MockRemoteSubscriptions() {
  auto subscriptions =
      std::make_unique<std::vector<commerce::CommerceSubscription>>();
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId1,
      commerce::ManagementType::kUserManaged, kMockTimestamp1));
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId2,
      commerce::ManagementType::kUserManaged, kMockTimestamp2));
  return subscriptions;
}

std::vector<
    SessionProtoStorage<commerce::CommerceSubscriptionProto>::KeyAndValue>
MockDbLoadResponse() {
  commerce::CommerceSubscriptionProto proto1;
  proto1.set_key(kKey2);
  proto1.set_tracking_id(kMockId2);
  proto1.set_subscription_type(
      commerce_subscription_db::
          CommerceSubscriptionContentProto_SubscriptionType_PRICE_TRACK);
  proto1.set_tracking_id_type(
      commerce_subscription_db::
          CommerceSubscriptionContentProto_TrackingIdType_PRODUCT_CLUSTER_ID);
  proto1.set_management_type(
      commerce_subscription_db::
          CommerceSubscriptionContentProto_SubscriptionManagementType_USER_MANAGED);
  proto1.set_timestamp(kMockTimestamp2);

  commerce::CommerceSubscriptionProto proto2;
  proto2.set_key(kKey3);
  proto2.set_tracking_id(kMockId3);
  proto2.set_subscription_type(
      commerce_subscription_db::
          CommerceSubscriptionContentProto_SubscriptionType_PRICE_TRACK);
  proto2.set_tracking_id_type(
      commerce_subscription_db::
          CommerceSubscriptionContentProto_TrackingIdType_PRODUCT_CLUSTER_ID);
  proto2.set_management_type(
      commerce_subscription_db::
          CommerceSubscriptionContentProto_SubscriptionManagementType_USER_MANAGED);
  proto2.set_timestamp(kMockTimestamp3);

  return std::vector<
      SessionProtoStorage<commerce::CommerceSubscriptionProto>::KeyAndValue>{
      {kKey2, proto1}, {kKey3, proto2}};
}

// Return one subscription with given id.
commerce::CommerceSubscription BuildSubscription(std::string subscription_id) {
  return commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, subscription_id,
      commerce::ManagementType::kUserManaged);
}

class MockProtoStorage
    : public SessionProtoStorage<commerce::CommerceSubscriptionProto> {
 public:
  MockProtoStorage() = default;
  ~MockProtoStorage() override = default;

  MOCK_METHOD(
      void,
      LoadContentWithPrefix,
      (const std::string& key_prefix,
       SessionProtoStorage<commerce::CommerceSubscriptionProto>::LoadCallback
           callback),
      (override));
  MOCK_METHOD(void,
              InsertContent,
              (const std::string& key,
               const commerce::CommerceSubscriptionProto& value,
               SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteOneEntry,
              (const std::string& key,
               SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(
      void,
      UpdateEntries,
      ((std::unique_ptr<std::vector<
            std::pair<std::string, commerce::CommerceSubscriptionProto>>>
            entries_to_update),
       std::unique_ptr<std::vector<std::string>> keys_to_remove,
       SessionProtoStorage<
           commerce::CommerceSubscriptionProto>::OperationCallback callback),
      (override));
  MOCK_METHOD(
      void,
      DeleteAllContent,
      (SessionProtoStorage<
          commerce::CommerceSubscriptionProto>::OperationCallback callback),
      (override));
  // TODO (crbug.com/1351599): Provide mock version of SessionProtoStorage so we
  // don't need to mock all methods here.
  MOCK_METHOD(void,
              LoadAllEntries,
              (SessionProtoStorage<
                  commerce::CommerceSubscriptionProto>::LoadCallback callback),
              (override));
  MOCK_METHOD(
      void,
      LoadOneEntry,
      (const std::string& key,
       SessionProtoStorage<commerce::CommerceSubscriptionProto>::LoadCallback
           callback),
      (override));
  MOCK_METHOD(void,
              PerformMaintenance,
              (const std::vector<std::string>& keys_to_keep,
               const std::string& key_substring_to_match,
               SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteContentWithPrefix,
              (const std::string& key_prefix,
               SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                   OperationCallback callback),
              (override));
  MOCK_METHOD(void, Destroy, (), (const, override));

  void MockLoadResponse(bool succeeded) {
    ON_CALL(*this, LoadContentWithPrefix)
        .WillByDefault(
            [succeeded](
                const std::string& key_prefix,
                SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                    LoadCallback callback) {
              std::move(callback).Run(succeeded, MockDbLoadResponse());
            });
  }

  void MockLoadAllResponse() {
    ON_CALL(*this, LoadAllEntries)
        .WillByDefault(
            [](SessionProtoStorage<
                commerce::CommerceSubscriptionProto>::LoadCallback callback) {
              std::move(callback).Run(true, MockDbLoadResponse());
            });
  }

  void MockOperationResult(bool succeeded) {
    ON_CALL(*this, InsertContent)
        .WillByDefault(
            [succeeded](
                const std::string& key,
                const commerce::CommerceSubscriptionProto& value,
                SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                    OperationCallback callback) {
              std::move(callback).Run(succeeded);
            });
    ON_CALL(*this, DeleteOneEntry)
        .WillByDefault(
            [succeeded](
                const std::string& key,
                SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                    OperationCallback callback) {
              std::move(callback).Run(succeeded);
            });
    ON_CALL(*this, UpdateEntries)
        .WillByDefault(
            [succeeded](
                std::unique_ptr<std::vector<std::pair<
                    std::string, commerce::CommerceSubscriptionProto>>>
                    entries_to_update,
                std::unique_ptr<std::vector<std::string>> keys_to_remove,
                SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                    OperationCallback callback) {
              std::move(callback).Run(succeeded);
            });
    ON_CALL(*this, DeleteAllContent)
        .WillByDefault(
            [succeeded](SessionProtoStorage<
                        commerce::CommerceSubscriptionProto>::OperationCallback
                            callback) { std::move(callback).Run(succeeded); });
  }

  void MockLoadOneEntryResponse(bool succeeded, bool empty_response) {
    ON_CALL(*this, LoadOneEntry)
        .WillByDefault(
            [succeeded, empty_response](
                const std::string& key,
                SessionProtoStorage<commerce::CommerceSubscriptionProto>::
                    LoadCallback callback) {
              std::move(callback).Run(
                  succeeded,
                  empty_response
                      ? std::vector<SessionProtoStorage<
                            commerce::CommerceSubscriptionProto>::KeyAndValue>()
                      : MockDbLoadResponse());
            });
  }
};

}  // namespace

namespace commerce {

class SubscriptionsStorageTest : public testing::Test {
 public:
  SubscriptionsStorageTest() = default;
  ~SubscriptionsStorageTest() override = default;

  void SetUp() override {
    proto_db_ = std::make_unique<MockProtoStorage>();
    proto_db_->MockLoadAllResponse();
    storage_ = std::make_unique<SubscriptionsStorage>(proto_db_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockProtoStorage> proto_db_;
  std::unique_ptr<SubscriptionsStorage> storage_;
};

TEST_F(SubscriptionsStorageTest, TestGetUniqueNonExistingSubscriptions) {
  proto_db_->MockLoadResponse(true);
  EXPECT_CALL(*proto_db_, LoadContentWithPrefix("PRICE_TRACK", _)).Times(1);

  base::RunLoop run_loop;
  storage_->GetUniqueNonExistingSubscriptions(
      MockIncomingSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(1, static_cast<int>(subscriptions->size()));
            auto subscription = (*subscriptions)[0];
            ASSERT_EQ(SubscriptionType::kPriceTrack, subscription.type);
            ASSERT_EQ(IdentifierType::kProductClusterId, subscription.id_type);
            ASSERT_EQ(ManagementType::kUserManaged,
                      subscription.management_type);
            ASSERT_EQ(kMockId1, subscription.id);
            ASSERT_EQ(kUnknownSubscriptionTimestamp, subscription.timestamp);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsStorageTest, TestGetUniqueExistingSubscriptions) {
  proto_db_->MockLoadResponse(true);
  EXPECT_CALL(*proto_db_, LoadContentWithPrefix("PRICE_TRACK", _)).Times(1);

  base::RunLoop run_loop;
  storage_->GetUniqueExistingSubscriptions(
      MockIncomingSubscriptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(1, static_cast<int>(subscriptions->size()));
            auto subscription = (*subscriptions)[0];
            ASSERT_EQ(SubscriptionType::kPriceTrack, subscription.type);
            ASSERT_EQ(IdentifierType::kProductClusterId, subscription.id_type);
            ASSERT_EQ(ManagementType::kUserManaged,
                      subscription.management_type);
            ASSERT_EQ(kMockId2, subscription.id);
            ASSERT_EQ(kMockTimestamp2, subscription.timestamp);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsStorageTest, TestDeleteAll) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));

  proto_db_->MockOperationResult(true);
  EXPECT_CALL(*proto_db_, DeleteAllContent).Times(1);
  storage_->DeleteAll();

  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));
}

TEST_F(SubscriptionsStorageTest, TestDeleteAll_Failed) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));

  proto_db_->MockOperationResult(false);
  EXPECT_CALL(*proto_db_, DeleteAllContent).Times(1);
  storage_->DeleteAll();

  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));
}

TEST_F(SubscriptionsStorageTest, TestUpdateStorage) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));

  proto_db_->MockLoadResponse(true);
  proto_db_->MockOperationResult(true);

  EXPECT_CALL(*proto_db_, LoadContentWithPrefix("PRICE_TRACK", _));
  EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _)).Times(1);

  base::RunLoop run_loop;
  storage_->UpdateStorage(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            run_loop->Quit();
          },
          &run_loop),
      MockRemoteSubscriptions());
  run_loop.Run();

  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));
}

TEST_F(SubscriptionsStorageTest,
       TestUpdateStorage_OneWithSameKeyDifferentTimestamp) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));

  // Mock that one local subscription has the same key but different timestamp
  // from a server-side subscription, in which case we need to delete the
  // local one and re-insert the server one.
  auto remote_subscriptions =
      std::make_unique<std::vector<CommerceSubscription>>();
  remote_subscriptions->push_back(CommerceSubscription(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      kMockId1, ManagementType::kUserManaged, kMockTimestamp1));
  remote_subscriptions->push_back(CommerceSubscription(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      kMockId2, ManagementType::kUserManaged, kMockTimestamp4));

  proto_db_->MockLoadResponse(true);
  proto_db_->MockOperationResult(true);

  EXPECT_CALL(*proto_db_, LoadContentWithPrefix("PRICE_TRACK", _));
  EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _)).Times(1);

  base::RunLoop run_loop;
  storage_->UpdateStorage(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            run_loop->Quit();
          },
          &run_loop),
      std::move(remote_subscriptions));
  run_loop.Run();

  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));
}

TEST_F(SubscriptionsStorageTest, TestUpdateStorage_LoadFailed) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));

  proto_db_->MockLoadResponse(false);
  proto_db_->MockOperationResult(true);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, LoadContentWithPrefix("PRICE_TRACK", _));
    EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _)).Times(1);
  }

  base::RunLoop run_loop;
  storage_->UpdateStorage(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            run_loop->Quit();
          },
          &run_loop),
      MockRemoteSubscriptions());
  run_loop.Run();

  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));
}

TEST_F(SubscriptionsStorageTest, TestUpdateStorage_OperationFailed) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));

  proto_db_->MockLoadResponse(true);
  proto_db_->MockOperationResult(false);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, LoadContentWithPrefix("PRICE_TRACK", _));
    EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _));
  }

  base::RunLoop run_loop;
  storage_->UpdateStorage(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status) {
            ASSERT_EQ(SubscriptionsRequestStatus::kStorageError, status);
            run_loop->Quit();
          },
          &run_loop),
      MockRemoteSubscriptions());
  run_loop.Run();

  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));
}

TEST_F(SubscriptionsStorageTest, UpdateStorageAndNotifyModifiedSubscriptions) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));

  proto_db_->MockLoadResponse(true);
  proto_db_->MockOperationResult(true);

  EXPECT_CALL(*proto_db_, LoadContentWithPrefix("PRICE_TRACK", _));
  EXPECT_CALL(*proto_db_, UpdateEntries(_, _, _)).Times(1);

  base::RunLoop run_loop;
  storage_->UpdateStorageAndNotifyModifiedSubscriptions(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop, SubscriptionsRequestStatus status,
             std::vector<CommerceSubscription> added_subs,
             std::vector<CommerceSubscription> removed_subs) {
            ASSERT_EQ(SubscriptionsRequestStatus::kSuccess, status);
            ASSERT_EQ(1, (int)added_subs.size());
            ASSERT_EQ(kMockId1, added_subs[0].id);
            ASSERT_EQ(1, (int)removed_subs.size());
            ASSERT_EQ(kMockId3, removed_subs[0].id);
            run_loop->Quit();
          },
          &run_loop),
      MockRemoteSubscriptions());
  run_loop.Run();

  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId3)));
}

TEST_F(SubscriptionsStorageTest, TestIsSubscribed) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));

  proto_db_->MockLoadOneEntryResponse(true, false);
  EXPECT_CALL(*proto_db_, LoadOneEntry(kKey1, _)).Times(1);

  base::RunLoop run_loop;
  storage_->IsSubscribed(
      CommerceSubscription(SubscriptionType::kPriceTrack,
                           IdentifierType::kProductClusterId, kMockId1,
                           ManagementType::kUserManaged),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool is_subscribed) {
            ASSERT_EQ(true, is_subscribed);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
}

TEST_F(SubscriptionsStorageTest, TestIsSubscribed_Failed) {
  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));

  proto_db_->MockLoadOneEntryResponse(false, false);
  EXPECT_CALL(*proto_db_, LoadOneEntry(kKey1, _)).Times(1);

  base::RunLoop run_loop;
  storage_->IsSubscribed(
      CommerceSubscription(SubscriptionType::kPriceTrack,
                           IdentifierType::kProductClusterId, kMockId1,
                           ManagementType::kUserManaged),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool is_subscribed) {
            ASSERT_EQ(false, is_subscribed);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId1)));
}

TEST_F(SubscriptionsStorageTest, TestIsSubscribed_EmptyResponse) {
  ASSERT_TRUE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));

  proto_db_->MockLoadOneEntryResponse(true, true);
  EXPECT_CALL(*proto_db_, LoadOneEntry(kKey2, _)).Times(1);

  base::RunLoop run_loop;
  storage_->IsSubscribed(
      CommerceSubscription(SubscriptionType::kPriceTrack,
                           IdentifierType::kProductClusterId, kMockId2,
                           ManagementType::kUserManaged),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool is_subscribed) {
            ASSERT_EQ(false, is_subscribed);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  ASSERT_FALSE(storage_->IsSubscribedFromCache(BuildSubscription(kMockId2)));
}

}  // namespace commerce
