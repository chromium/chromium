// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/discount_infos_storage.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/discount_infos_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

const char kDeleteUrl1[] = "http://example.com/delete1";
const char kDeleteUrl2[] = "http://example.com/delete2";

const char kProductUrl1[] = "http://example.com/product1";
const char kProductUrl2[] = "http://example.com/product2";
const char kMerchantUrl[] = "http://example.com/";

const char kDiscountLanguageCode[] = "en-US";
const char kDiscountDetail[] = "details";
const char kDiscountTerms[] = "terms";
const char kDiscountCode1[] = "discount code 1";
const char kDiscountCode2[] = "discount code 2";
const char kDiscountCode3[] = "discount code 3";
const uint64_t kDiscountIdInDb1 = 333;
const uint64_t kDiscountIdInDb2 = 444;
const uint64_t kDiscountIdInDb3 = 555;
const uint64_t kDiscountOfferId = 123456;
const double kDiscountExpiryTime = 1000000000000;

SessionProtoStorage<commerce::DiscountInfosContent>::KeyAndValue
BuildDiscountInfo(
    const std::string& url,
    const std::vector<std::pair<uint64_t, std::string>>& ids_and_codes) {
  double timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() + 100;
  commerce::DiscountInfosContent proto;
  proto.set_key(url);
  for (const auto& [id, code] : ids_and_codes) {
    discount_infos_db::DiscountInfoContent* discount_proto =
        proto.add_discounts();
    discount_proto->set_id(id);
    discount_proto->set_type(
        discount_infos_db::DiscountInfoContent_Type_FREE_LISTING_WITH_CODE);
    discount_proto->set_language_code(kDiscountLanguageCode);
    discount_proto->set_language_code(kDiscountLanguageCode);
    discount_proto->set_description_detail(kDiscountDetail);
    discount_proto->set_terms_and_conditions(kDiscountTerms);
    discount_proto->set_expiry_time_sec(timestamp);
    discount_proto->set_discount_code(code);
    discount_proto->set_offer_id(kDiscountOfferId);
  }
  return {url, proto};
}

std::vector<SessionProtoStorage<commerce::DiscountInfosContent>::KeyAndValue>
MockDbLoadResponseMultipleMatches(const std::string& url1,
                                  bool discount1_expired,
                                  bool discount2_expired,
                                  const std::string& url2,
                                  bool discount3_expired) {
  double expired_timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() - 100;
  double unexpired_timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() + 100;
  commerce::DiscountInfosContent proto1;
  proto1.set_key(url1);
  discount_infos_db::DiscountInfoContent* discount_proto1 =
      proto1.add_discounts();
  discount_proto1->set_id(kDiscountIdInDb1);
  discount_proto1->set_type(
      discount_infos_db::DiscountInfoContent_Type_FREE_LISTING_WITH_CODE);
  discount_proto1->set_language_code(kDiscountLanguageCode);
  discount_proto1->set_description_detail(kDiscountDetail);
  discount_proto1->set_terms_and_conditions(kDiscountTerms);
  discount_proto1->set_expiry_time_sec(discount1_expired ? expired_timestamp
                                                         : unexpired_timestamp);
  discount_proto1->set_discount_code(kDiscountCode1);
  discount_proto1->set_offer_id(kDiscountOfferId);

  discount_infos_db::DiscountInfoContent* discount_proto2 =
      proto1.add_discounts();
  discount_proto2->set_id(kDiscountIdInDb2);
  discount_proto2->set_type(
      discount_infos_db::DiscountInfoContent_Type_FREE_LISTING_WITH_CODE);
  discount_proto2->set_language_code(kDiscountLanguageCode);
  discount_proto2->set_description_detail(kDiscountDetail);
  discount_proto2->set_expiry_time_sec(discount2_expired ? expired_timestamp
                                                         : unexpired_timestamp);
  discount_proto2->set_discount_code(kDiscountCode2);
  discount_proto2->set_offer_id(kDiscountOfferId);

  commerce::DiscountInfosContent proto2;
  proto2.set_key(url2);
  discount_infos_db::DiscountInfoContent* discount_proto3 =
      proto2.add_discounts();
  discount_proto3->set_id(kDiscountIdInDb3);
  discount_proto3->set_type(
      discount_infos_db::DiscountInfoContent_Type_FREE_LISTING_WITH_CODE);
  discount_proto3->set_language_code(kDiscountLanguageCode);
  discount_proto3->set_description_detail(kDiscountDetail);
  discount_proto3->set_expiry_time_sec(discount3_expired ? expired_timestamp
                                                         : unexpired_timestamp);
  discount_proto3->set_discount_code(kDiscountCode3);
  discount_proto3->set_offer_id(kDiscountOfferId);

  return std::vector<
      SessionProtoStorage<commerce::DiscountInfosContent>::KeyAndValue>{
      {url1, proto1}, {url2, proto2}};
}

std::vector<SessionProtoStorage<commerce::DiscountInfosContent>::KeyAndValue>
MockDbLoadResponseSingleMatch(const std::string& url,
                              bool discount1_expired,
                              bool discount2_expired) {
  double expired_timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() - 100;
  double unexpired_timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() + 100;
  commerce::DiscountInfosContent proto;
  proto.set_key(url);
  discount_infos_db::DiscountInfoContent* discount_proto1 =
      proto.add_discounts();
  discount_proto1->set_id(kDiscountIdInDb1);
  discount_proto1->set_type(
      discount_infos_db::DiscountInfoContent_Type_FREE_LISTING_WITH_CODE);
  discount_proto1->set_language_code(kDiscountLanguageCode);
  discount_proto1->set_description_detail(kDiscountDetail);
  discount_proto1->set_terms_and_conditions(kDiscountTerms);
  discount_proto1->set_expiry_time_sec(discount1_expired ? expired_timestamp
                                                         : unexpired_timestamp);
  discount_proto1->set_discount_code(kDiscountCode1);
  discount_proto1->set_offer_id(kDiscountOfferId);

  discount_infos_db::DiscountInfoContent* discount_proto2 =
      proto.add_discounts();
  discount_proto2->set_id(kDiscountIdInDb2);
  discount_proto2->set_type(
      discount_infos_db::DiscountInfoContent_Type_FREE_LISTING_WITH_CODE);
  discount_proto2->set_language_code(kDiscountLanguageCode);
  discount_proto2->set_description_detail(kDiscountDetail);
  discount_proto2->set_expiry_time_sec(discount2_expired ? expired_timestamp
                                                         : unexpired_timestamp);
  discount_proto2->set_discount_code(kDiscountCode2);
  discount_proto2->set_offer_id(kDiscountOfferId);

  return std::vector<
      SessionProtoStorage<commerce::DiscountInfosContent>::KeyAndValue>{
      {url, proto}};
}

class MockProtoStorage
    : public SessionProtoStorage<commerce::DiscountInfosContent> {
 public:
  MockProtoStorage() = default;
  ~MockProtoStorage() override = default;

  MOCK_METHOD(void,
              LoadContentWithPrefix,
              (const std::string& key_prefix,
               SessionProtoStorage<commerce::DiscountInfosContent>::LoadCallback
                   callback),
              (override));
  MOCK_METHOD(
      void,
      InsertContent,
      (const std::string& key,
       const commerce::DiscountInfosContent& value,
       SessionProtoStorage<commerce::DiscountInfosContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(
      void,
      DeleteOneEntry,
      (const std::string& key,
       SessionProtoStorage<commerce::DiscountInfosContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(void,
              DeleteAllContent,
              (SessionProtoStorage<
                  commerce::DiscountInfosContent>::OperationCallback callback),
              (override));
  MOCK_METHOD(void,
              LoadAllEntries,
              (SessionProtoStorage<commerce::DiscountInfosContent>::LoadCallback
                   callback),
              (override));
  MOCK_METHOD(void,
              LoadOneEntry,
              (const std::string& key,
               SessionProtoStorage<commerce::DiscountInfosContent>::LoadCallback
                   callback),
              (override));
  MOCK_METHOD(
      void,
      UpdateEntries,
      ((std::unique_ptr<
           std::vector<std::pair<std::string, commerce::DiscountInfosContent>>>
            entries_to_update),
       std::unique_ptr<std::vector<std::string>> keys_to_remove,
       SessionProtoStorage<commerce::DiscountInfosContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(
      void,
      PerformMaintenance,
      (const std::vector<std::string>& keys_to_keep,
       const std::string& key_substring_to_match,
       SessionProtoStorage<commerce::DiscountInfosContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(
      void,
      DeleteContentWithPrefix,
      (const std::string& key_prefix,
       SessionProtoStorage<commerce::DiscountInfosContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(void, Destroy, (), (const, override));

  void MockLoadAllResponseSingleMatch(const std::string& url,
                                      bool succeeded,
                                      bool discount1_expired,
                                      bool discount2_expired) {
    ON_CALL(*this, LoadContentWithPrefix)
        .WillByDefault(
            [url, succeeded, discount1_expired, discount2_expired](
                std::string key_prefix,
                SessionProtoStorage<
                    commerce::DiscountInfosContent>::LoadCallback callback) {
              std::move(callback).Run(
                  succeeded, MockDbLoadResponseSingleMatch(
                                 url, discount1_expired, discount2_expired));
            });
  }
  void MockLoadAllResponseMultipleMatches(bool succeeded,
                                          const std::string& url1,
                                          bool discount1_expired,
                                          bool discount2_expired,
                                          const std::string& url2,
                                          bool discount3_expired) {
    ON_CALL(*this, LoadContentWithPrefix)
        .WillByDefault(
            [succeeded, url1, discount1_expired, discount2_expired, url2,
             discount3_expired](
                std::string key_prefix,
                SessionProtoStorage<
                    commerce::DiscountInfosContent>::LoadCallback callback) {
              std::move(callback).Run(
                  succeeded, MockDbLoadResponseMultipleMatches(
                                 url1, discount1_expired, discount2_expired,
                                 url2, discount3_expired));
            });
  }
};

}  // namespace

namespace commerce {

class DiscountInfosStorageTest : public testing::Test {
 public:
  DiscountInfosStorageTest() = default;
  ~DiscountInfosStorageTest() override = default;

  void SetUp() override {
    proto_db_ = std::make_unique<MockProtoStorage>();
    storage_ = std::make_unique<DiscountInfosStorage>(proto_db_.get(), nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockProtoStorage> proto_db_;
  std::unique_ptr<DiscountInfosStorage> storage_;
};

TEST_F(DiscountInfosStorageTest,
       TestLoadDiscountsWithPrefix_AllDiscountsUnexpired) {
  proto_db_->MockLoadAllResponseSingleMatch(kProductUrl1, /*succeeded=*/true,
                                            /*discount1_expired=*/false,
                                            /*discount2_expired=*/false);

  EXPECT_CALL(*proto_db_, LoadContentWithPrefix);

  base::RunLoop run_loop;
  storage_->LoadDiscountsWithPrefix(
      GURL(kProductUrl1),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const std::vector<DiscountInfo> results) {
            ASSERT_EQ(2, (int)results.size());
            ASSERT_EQ(kDiscountIdInDb1, results[0].id);
            ASSERT_EQ(DiscountType::kFreeListingWithCode, results[0].type);
            ASSERT_EQ(kDiscountLanguageCode, results[0].language_code);
            ASSERT_EQ(kDiscountDetail, results[0].description_detail);
            ASSERT_EQ(kDiscountTerms, results[0].terms_and_conditions);
            ASSERT_EQ(kDiscountCode1, results[0].discount_code);
            ASSERT_EQ(kDiscountOfferId, results[0].offer_id);

            ASSERT_EQ(kDiscountIdInDb2, results[1].id);
            ASSERT_EQ(std::nullopt, results[1].terms_and_conditions);
            ASSERT_EQ(kDiscountCode2, results[1].discount_code);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(DiscountInfosStorageTest,
       TestLoadDiscountsWithPrefix_AllDiscountsWithDuplicateDiscountCode) {

  ON_CALL(*proto_db_, LoadContentWithPrefix)
      .WillByDefault(
          [](std::string key_prefix,
             SessionProtoStorage<commerce::DiscountInfosContent>::LoadCallback
                 callback) {
            // kProductUrl1 has 2 distinct discounts: kDiscountCode1 and
            // kDiscountCode2. kProductUrl2 has 1 discount with kDiscountCode1,
            // which is a duplicate of one of the discounts in kProductUrl1.
            std::move(callback).Run(
                true, {BuildDiscountInfo(kProductUrl1,
                                         {{kDiscountIdInDb1, kDiscountCode1},
                                          {kDiscountIdInDb2, kDiscountCode2}}),
                       BuildDiscountInfo(kProductUrl2, {{kDiscountIdInDb3,
                                                         kDiscountCode1}})});
          });

  EXPECT_CALL(*proto_db_, LoadContentWithPrefix);
  EXPECT_CALL(*proto_db_, InsertContent).Times(0);
  EXPECT_CALL(*proto_db_, DeleteOneEntry).Times(0);

  base::RunLoop run_loop;
  storage_->LoadDiscountsWithPrefix(
      GURL(kMerchantUrl),
      base::BindOnce([](const GURL& url,
                        const std::vector<DiscountInfo> results) {
        ASSERT_EQ(2, (int)results.size());

        ASSERT_EQ(kDiscountIdInDb1, results[0].id);
        ASSERT_EQ(DiscountType::kFreeListingWithCode, results[0].type);
        ASSERT_EQ(kDiscountLanguageCode, results[0].language_code);
        ASSERT_EQ(kDiscountDetail, results[0].description_detail);
        ASSERT_EQ(kDiscountTerms, results[0].terms_and_conditions);
        ASSERT_EQ(kDiscountCode1, results[0].discount_code);
        ASSERT_EQ(kDiscountOfferId, results[0].offer_id);

        ASSERT_EQ(kDiscountIdInDb2, results[1].id);
        ASSERT_EQ(kDiscountTerms, results[1].terms_and_conditions);
        ASSERT_EQ(kDiscountCode2, results[1].discount_code);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DiscountInfosStorageTest,
       TestLoadDiscountsWithPrefix_MultipleMatches_AllDiscountsUnexpired) {
  proto_db_->MockLoadAllResponseMultipleMatches(
      /*succeeded=*/true, kProductUrl1, /*discount1_expired=*/false,
      /*discount2_expired=*/false, kProductUrl2, /*discount3_expired=*/false);

  EXPECT_CALL(*proto_db_, LoadContentWithPrefix);
  EXPECT_CALL(*proto_db_, InsertContent).Times(0);
  EXPECT_CALL(*proto_db_, DeleteOneEntry).Times(0);

  base::RunLoop run_loop;
  storage_->LoadDiscountsWithPrefix(
      GURL(kMerchantUrl),
      base::BindOnce([](const GURL& url,
                        const std::vector<DiscountInfo> results) {
        ASSERT_EQ(3, (int)results.size());

        ASSERT_EQ(kDiscountIdInDb1, results[0].id);
        ASSERT_EQ(DiscountType::kFreeListingWithCode, results[0].type);
        ASSERT_EQ(kDiscountLanguageCode, results[0].language_code);
        ASSERT_EQ(kDiscountDetail, results[0].description_detail);
        ASSERT_EQ(kDiscountTerms, results[0].terms_and_conditions);
        ASSERT_EQ(kDiscountCode1, results[0].discount_code);
        ASSERT_EQ(kDiscountOfferId, results[0].offer_id);

        ASSERT_EQ(kDiscountIdInDb2, results[1].id);
        ASSERT_EQ(std::nullopt, results[1].terms_and_conditions);

        ASSERT_EQ(kDiscountIdInDb3, results[2].id);
        ASSERT_EQ(std::nullopt, results[2].terms_and_conditions);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DiscountInfosStorageTest,
       TestLoadDiscountsWithPrefix_MultipleMatches_AllDiscountsExpired) {
  proto_db_->MockLoadAllResponseMultipleMatches(
      /*succeeded=*/true, kProductUrl1, /*discount1_expired=*/true,
      /*discount2_expired=*/true, kProductUrl2, /*discount3_expired=*/true);
  {
    InSequence s;
    EXPECT_CALL(*proto_db_, LoadContentWithPrefix);
    EXPECT_CALL(*proto_db_, DeleteOneEntry(kProductUrl1, _));
    EXPECT_CALL(*proto_db_, DeleteOneEntry(kProductUrl2, _));
  }
  base::RunLoop run_loop;
  storage_->LoadDiscountsWithPrefix(
      GURL(kMerchantUrl),
      base::BindOnce([](const GURL& url,
                        const std::vector<DiscountInfo> results) {
        ASSERT_EQ(kMerchantUrl, url.spec());
        ASSERT_EQ(0, (int)results.size());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DiscountInfosStorageTest,
       TestLoadDiscountsWithPrefix_PartDiscountsExpired) {
  proto_db_->MockLoadAllResponseSingleMatch(kProductUrl1, /*succeeded=*/true,
                                            /*discount1_expired=*/false,
                                            /*discount2_expired=*/true);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, LoadContentWithPrefix);
    EXPECT_CALL(*proto_db_, InsertContent(kProductUrl1, _, _));
  }

  base::RunLoop run_loop;
  storage_->LoadDiscountsWithPrefix(
      GURL(kProductUrl1),
      base::BindOnce([](const GURL& url,
                        const std::vector<DiscountInfo> results) {
        ASSERT_EQ(1, (int)results.size());
        ASSERT_EQ(kDiscountIdInDb1, results[0].id);
        ASSERT_EQ(DiscountType::kFreeListingWithCode, results[0].type);
        ASSERT_EQ(kDiscountLanguageCode, results[0].language_code);
        ASSERT_EQ(kDiscountDetail, results[0].description_detail);
        ASSERT_EQ(kDiscountTerms, results[0].terms_and_conditions);
        ASSERT_EQ(kDiscountCode1, results[0].discount_code);
        ASSERT_EQ(kDiscountOfferId, results[0].offer_id);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DiscountInfosStorageTest,
       TestLoadDiscountsWithPrefix_MultipleMatches_PartDiscountsExpired) {
  proto_db_->MockLoadAllResponseMultipleMatches(
      /*succeeded=*/true, kProductUrl1, /*discount1_expired=*/true,
      /*discount2_expired=*/false, kProductUrl2, /*discount3_expired=*/true);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, LoadContentWithPrefix);
    EXPECT_CALL(*proto_db_, InsertContent(kProductUrl1, _, _));
    EXPECT_CALL(*proto_db_, DeleteOneEntry(kProductUrl2, _));
  }

  base::RunLoop run_loop;
  storage_->LoadDiscountsWithPrefix(
      GURL(kMerchantUrl),
      base::BindOnce([](const GURL& url,
                        const std::vector<DiscountInfo> results) {
        ASSERT_EQ(1, (int)results.size());
        ASSERT_EQ(kDiscountIdInDb2, results[0].id);
        ASSERT_EQ(DiscountType::kFreeListingWithCode, results[0].type);
        ASSERT_EQ(kDiscountLanguageCode, results[0].language_code);
        ASSERT_EQ(kDiscountDetail, results[0].description_detail);
        ASSERT_EQ(std::nullopt, results[0].terms_and_conditions);
        ASSERT_EQ(kDiscountCode2, results[0].discount_code);
        ASSERT_EQ(kDiscountOfferId, results[0].offer_id);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DiscountInfosStorageTest, TestLoadDiscountsWithPrefix_AllDiscountsExpired) {
  proto_db_->MockLoadAllResponseSingleMatch(kProductUrl1, /*succeeded=*/true,
                                            /*discount1_expired=*/true,
                                            /*discount2_expired=*/true);
  EXPECT_CALL(*proto_db_, DeleteOneEntry).Times(1);

  base::RunLoop run_loop;
  storage_->LoadDiscountsWithPrefix(
      GURL(kProductUrl1),
      base::BindOnce([](const GURL& url,
                        const std::vector<DiscountInfo> results) {
        ASSERT_EQ(0, (int)results.size());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DiscountInfosStorageTest, TestOnURLsDeleted_DeleteAll) {
  EXPECT_CALL(*proto_db_, DeleteAllContent).Times(1);
  EXPECT_CALL(*proto_db_, DeleteOneEntry).Times(0);

  storage_->OnHistoryDeletions(nullptr, history::DeletionInfo::ForAllHistory());
}

TEST_F(DiscountInfosStorageTest, TestOnURLsDeleted_DeleteUrls) {
  EXPECT_CALL(*proto_db_, DeleteAllContent).Times(0);
  EXPECT_CALL(*proto_db_, DeleteOneEntry(kDeleteUrl1, _)).Times(1);
  EXPECT_CALL(*proto_db_, DeleteOneEntry(kDeleteUrl2, _)).Times(1);

  storage_->OnHistoryDeletions(
      nullptr,
      history::DeletionInfo::ForUrls({history::URLRow(GURL(kDeleteUrl1)),
                                      history::URLRow(GURL(kDeleteUrl2))},
                                     {}));
}

TEST_F(DiscountInfosStorageTest, TestSaveDiscounts) {
  EXPECT_CALL(*proto_db_, InsertContent).Times(1);

  DiscountInfo discount_info;
  discount_info.id = kDiscountIdInDb1;
  discount_info.type = DiscountType::kFreeListingWithCode;
  discount_info.language_code = kDiscountLanguageCode;
  discount_info.description_detail = kDiscountDetail;
  discount_info.terms_and_conditions = kDiscountTerms;
  discount_info.discount_code = kDiscountCode1;
  discount_info.offer_id = kDiscountOfferId;
  discount_info.expiry_time_sec = kDiscountExpiryTime;
  storage_->SaveDiscounts(GURL(kProductUrl1), {discount_info});
}

TEST_F(DiscountInfosStorageTest, TestSaveDiscounts_EmptyDiscountInfo) {
  EXPECT_CALL(*proto_db_, InsertContent).Times(0);
  EXPECT_CALL(*proto_db_, DeleteOneEntry(kProductUrl1, _)).Times(1);

  storage_->SaveDiscounts(GURL(kProductUrl1), {});
}

TEST_F(DiscountInfosStorageTest, TestSaveDiscounts_NoExpiryTime) {
  EXPECT_CALL(*proto_db_, InsertContent).Times(0);
  EXPECT_CALL(*proto_db_, DeleteOneEntry(kProductUrl1, _)).Times(1);

  DiscountInfo discount_info;
  discount_info.id = kDiscountIdInDb1;
  discount_info.type = DiscountType::kFreeListingWithCode;
  discount_info.language_code = kDiscountLanguageCode;
  discount_info.description_detail = kDiscountDetail;
  discount_info.terms_and_conditions = kDiscountTerms;
  discount_info.discount_code = kDiscountCode1;
  discount_info.offer_id = kDiscountOfferId;
  storage_->SaveDiscounts(GURL(kProductUrl1), {discount_info});
}
}  // namespace commerce
