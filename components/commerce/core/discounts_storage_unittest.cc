// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/discounts_storage.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/history/core/browser/history_types.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

const char kDiscountsUrlFromServer[] =
    "http://example.com/discounts_from_server";
const char kDiscountsUrlToCheckInDb[] =
    "http://example.com/discounts_to_check_in_db";
const char kDiscountsUrlInDb[] = "http://example.com/discounts_db";
const char kDiscountLanguageCode[] = "en-US";
const char kDiscountDetail[] = "details";
const char kDiscountTerms[] = "terms";
const char kDiscountValueText[] = "10% off";
const double kDiscountExpiryTime = 1000000;
const char kDiscountCode[] = "discount code";
const uint64_t kDiscountIdFromServer = 111;
const uint64_t kDiscountIdInDb1 = 333;
const uint64_t kDiscountIdInDb2 = 444;
const uint64_t kDiscountOfferId = 123456;
const char kDeleteUrl1[] = "http://example.com/delete1";
const char kDeleteUrl2[] = "http://example.com/delete2";

commerce::DiscountsMap MockServerResults() {
  commerce::DiscountInfo info;
  info.cluster_type = commerce::DiscountClusterType::kOfferLevel;
  info.id = kDiscountIdFromServer;
  info.type = commerce::DiscountType::kFreeListingWithCode;
  info.language_code = kDiscountLanguageCode;
  info.description_detail = kDiscountDetail;
  info.value_in_text = kDiscountValueText;
  info.expiry_time_sec = kDiscountExpiryTime;
  info.is_merchant_wide = false;
  info.discount_code = kDiscountCode;
  info.offer_id = kDiscountOfferId;

  std::vector<commerce::DiscountInfo> infos;
  infos.push_back(info);

  commerce::DiscountsMap map;
  map[GURL(kDiscountsUrlFromServer)] = infos;
  return map;
}

MATCHER(ExpectedServerProto, "") {
  if (arg.key() != kDiscountsUrlFromServer || arg.discounts_size() != 1) {
    return false;
  }
  discounts_db::DiscountContent discount_proto = arg.discounts(0);
  return discount_proto.cluster_type() ==
             discounts_db::DiscountContent_ClusterType_OFFER_LEVEL &&
         discount_proto.id() == kDiscountIdFromServer &&
         discount_proto.type() ==
             discounts_db::DiscountContent_Type_FREE_LISTING_WITH_CODE &&
         discount_proto.language_code() == kDiscountLanguageCode &&
         discount_proto.description_detail() == kDiscountDetail &&
         !discount_proto.has_terms_and_conditions() &&
         discount_proto.value_in_text() == kDiscountValueText &&
         discount_proto.expiry_time_sec() == kDiscountExpiryTime &&
         !discount_proto.is_merchant_wide() &&
         discount_proto.discount_code() == kDiscountCode &&
         discount_proto.offer_id() == kDiscountOfferId;
}

std::vector<SessionProtoStorage<commerce::DiscountsContent>::KeyAndValue>
MockDbLoadResponse(bool discount1_expired, bool discount2_expired) {
  double expired_timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() - 100;
  double unexpired_timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() + 100;
  commerce::DiscountsContent proto;
  proto.set_key(kDiscountsUrlInDb);
  discounts_db::DiscountContent* discount_proto1 = proto.add_discounts();
  discount_proto1->set_cluster_type(
      discounts_db::DiscountContent_ClusterType_OFFER_LEVEL);
  discount_proto1->set_id(kDiscountIdInDb1);
  discount_proto1->set_type(
      discounts_db::DiscountContent_Type_FREE_LISTING_WITH_CODE);
  discount_proto1->set_language_code(kDiscountLanguageCode);
  discount_proto1->set_description_detail(kDiscountDetail);
  discount_proto1->set_terms_and_conditions(kDiscountTerms);
  discount_proto1->set_value_in_text(kDiscountValueText);
  discount_proto1->set_expiry_time_sec(discount1_expired ? expired_timestamp
                                                         : unexpired_timestamp);
  discount_proto1->set_is_merchant_wide(true);
  discount_proto1->set_discount_code(kDiscountCode);
  discount_proto1->set_offer_id(kDiscountOfferId);
  // Expired discount.
  discounts_db::DiscountContent* discount_proto2 = proto.add_discounts();
  discount_proto2->set_cluster_type(
      discounts_db::DiscountContent_ClusterType_OFFER_LEVEL);
  discount_proto2->set_id(kDiscountIdInDb2);
  discount_proto2->set_type(
      discounts_db::DiscountContent_Type_FREE_LISTING_WITH_CODE);
  discount_proto2->set_language_code(kDiscountLanguageCode);
  discount_proto2->set_description_detail(kDiscountDetail);
  discount_proto2->set_value_in_text(kDiscountValueText);
  discount_proto2->set_expiry_time_sec(discount2_expired ? expired_timestamp
                                                         : unexpired_timestamp);
  discount_proto2->set_is_merchant_wide(false);
  discount_proto2->set_discount_code(kDiscountCode);
  discount_proto2->set_offer_id(kDiscountOfferId);

  return std::vector<
      SessionProtoStorage<commerce::DiscountsContent>::KeyAndValue>{
      {kDiscountsUrlInDb, proto}};
}

class MockProtoStorage
    : public SessionProtoStorage<commerce::DiscountsContent> {
 public:
  MockProtoStorage() = default;
  ~MockProtoStorage() override = default;

  MOCK_METHOD(
      void,
      LoadContentWithPrefix,
      (const std::string& key_prefix,
       SessionProtoStorage<commerce::DiscountsContent>::LoadCallback callback),
      (override));
  MOCK_METHOD(
      void,
      InsertContent,
      (const std::string& key,
       const commerce::DiscountsContent& value,
       SessionProtoStorage<commerce::DiscountsContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(
      void,
      DeleteOneEntry,
      (const std::string& key,
       SessionProtoStorage<commerce::DiscountsContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(void,
              DeleteAllContent,
              (SessionProtoStorage<
                  commerce::DiscountsContent>::OperationCallback callback),
              (override));
  MOCK_METHOD(
      void,
      LoadAllEntries,
      (SessionProtoStorage<commerce::DiscountsContent>::LoadCallback callback),
      (override));
  MOCK_METHOD(
      void,
      LoadOneEntry,
      (const std::string& key,
       SessionProtoStorage<commerce::DiscountsContent>::LoadCallback callback),
      (override));
  MOCK_METHOD(
      void,
      PerformMaintenance,
      (const std::vector<std::string>& keys_to_keep,
       const std::string& key_substring_to_match,
       SessionProtoStorage<commerce::DiscountsContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(
      void,
      DeleteContentWithPrefix,
      (const std::string& key_prefix,
       SessionProtoStorage<commerce::DiscountsContent>::OperationCallback
           callback),
      (override));
  MOCK_METHOD(void, Destroy, (), (const, override));

  void MockLoadAllResponse(bool succeeded,
                           bool discount1_expired,
                           bool discount2_expired) {
    ON_CALL(*this, LoadAllEntries)
        .WillByDefault(
            [succeeded, discount1_expired, discount2_expired](
                SessionProtoStorage<commerce::DiscountsContent>::LoadCallback
                    callback) {
              std::move(callback).Run(
                  succeeded,
                  MockDbLoadResponse(discount1_expired, discount2_expired));
            });
  }
};

}  // namespace

namespace commerce {

class DiscountsStorageTest : public testing::Test {
 public:
  DiscountsStorageTest() = default;
  ~DiscountsStorageTest() override = default;

  void SetUp() override {
    proto_db_ = std::make_unique<MockProtoStorage>();
    storage_ = std::make_unique<DiscountsStorage>(proto_db_.get(), nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockProtoStorage> proto_db_;
  std::unique_ptr<DiscountsStorage> storage_;
};

TEST_F(DiscountsStorageTest, TestHandleServerDiscounts_NoUrlsToCheck) {
  EXPECT_CALL(*proto_db_,
              InsertContent(kDiscountsUrlFromServer, ExpectedServerProto(), _));
  EXPECT_CALL(*proto_db_, LoadAllEntries).Times(0);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  storage_->HandleServerDiscounts(
      std::vector<std::string>(), MockServerResults(),
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());
            auto discounts = map.find(GURL(kDiscountsUrlFromServer))->second;
            ASSERT_EQ(1, (int)discounts.size());
            ASSERT_EQ(kDiscountIdFromServer, discounts[0].id);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);
}

TEST_F(DiscountsStorageTest, TestHandleServerDiscounts_FailToLoad) {
  proto_db_->MockLoadAllResponse(false, false, false);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, InsertContent(kDiscountsUrlFromServer,
                                          ExpectedServerProto(), _));
    EXPECT_CALL(*proto_db_, LoadAllEntries);
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  storage_->HandleServerDiscounts(
      std::vector<std::string>{kDiscountsUrlToCheckInDb, kDiscountsUrlInDb},
      MockServerResults(),
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());
            auto discounts = map.find(GURL(kDiscountsUrlFromServer))->second;
            ASSERT_EQ(1, (int)discounts.size());
            ASSERT_EQ(kDiscountIdFromServer, discounts[0].id);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);
}

TEST_F(DiscountsStorageTest, TestHandleServerDiscounts_AllDiscountsUnexpired) {
  proto_db_->MockLoadAllResponse(true, false, false);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, InsertContent(kDiscountsUrlFromServer,
                                          ExpectedServerProto(), _));
    EXPECT_CALL(*proto_db_, LoadAllEntries);
    EXPECT_CALL(*proto_db_, InsertContent).Times(0);
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  storage_->HandleServerDiscounts(
      std::vector<std::string>{kDiscountsUrlToCheckInDb, kDiscountsUrlInDb},
      MockServerResults(),
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(2, (int)map.size());
            auto discounts = map.find(GURL(kDiscountsUrlInDb))->second;
            ASSERT_EQ(2, (int)discounts.size());

            ASSERT_EQ(DiscountClusterType::kOfferLevel,
                      discounts[0].cluster_type);
            ASSERT_EQ(kDiscountIdInDb1, discounts[0].id);
            ASSERT_EQ(DiscountType::kFreeListingWithCode, discounts[0].type);
            ASSERT_EQ(kDiscountLanguageCode, discounts[0].language_code);
            ASSERT_EQ(kDiscountDetail, discounts[0].description_detail);
            ASSERT_EQ(kDiscountTerms, discounts[0].terms_and_conditions);
            ASSERT_EQ(kDiscountValueText, discounts[0].value_in_text);
            ASSERT_EQ(kDiscountCode, discounts[0].discount_code);
            ASSERT_EQ(true, discounts[0].is_merchant_wide);
            ASSERT_EQ(kDiscountOfferId, discounts[0].offer_id);

            ASSERT_EQ(kDiscountIdInDb2, discounts[1].id);
            ASSERT_EQ(absl::nullopt, discounts[1].terms_and_conditions);
            ASSERT_EQ(false, discounts[1].is_merchant_wide);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 2);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 1, 1);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 3, 1);
}

TEST_F(DiscountsStorageTest, TestHandleServerDiscounts_AllDiscountsExpired) {
  proto_db_->MockLoadAllResponse(true, true, true);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, InsertContent(kDiscountsUrlFromServer,
                                          ExpectedServerProto(), _));
    EXPECT_CALL(*proto_db_, LoadAllEntries);
    EXPECT_CALL(*proto_db_, DeleteOneEntry(kDiscountsUrlInDb, _));
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  storage_->HandleServerDiscounts(
      std::vector<std::string>{kDiscountsUrlToCheckInDb, kDiscountsUrlInDb},
      MockServerResults(),
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());
            auto discounts = map.find(GURL(kDiscountsUrlFromServer))->second;
            ASSERT_EQ(1, (int)discounts.size());
            ASSERT_EQ(kDiscountIdFromServer, discounts[0].id);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 2);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 2, 1);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 3, 1);
}

TEST_F(DiscountsStorageTest, TestHandleServerDiscounts_PartDiscountsExpired) {
  proto_db_->MockLoadAllResponse(true, false, true);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, InsertContent(kDiscountsUrlFromServer,
                                          ExpectedServerProto(), _));
    EXPECT_CALL(*proto_db_, LoadAllEntries);
    EXPECT_CALL(*proto_db_, InsertContent(kDiscountsUrlInDb, _, _));
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  storage_->HandleServerDiscounts(
      std::vector<std::string>{kDiscountsUrlToCheckInDb, kDiscountsUrlInDb},
      MockServerResults(),
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(2, (int)map.size());
            auto discounts = map.find(GURL(kDiscountsUrlInDb))->second;
            ASSERT_EQ(1, (int)discounts.size());

            ASSERT_EQ(DiscountClusterType::kOfferLevel,
                      discounts[0].cluster_type);
            ASSERT_EQ(kDiscountIdInDb1, discounts[0].id);
            ASSERT_EQ(DiscountType::kFreeListingWithCode, discounts[0].type);
            ASSERT_EQ(kDiscountLanguageCode, discounts[0].language_code);
            ASSERT_EQ(kDiscountDetail, discounts[0].description_detail);
            ASSERT_EQ(kDiscountTerms, discounts[0].terms_and_conditions);
            ASSERT_EQ(kDiscountValueText, discounts[0].value_in_text);
            ASSERT_EQ(kDiscountCode, discounts[0].discount_code);
            ASSERT_EQ(true, discounts[0].is_merchant_wide);
            ASSERT_EQ(kDiscountOfferId, discounts[0].offer_id);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 2);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 1, 1);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 3, 1);
}

TEST_F(DiscountsStorageTest, TestHandleServerDiscounts_NoDiscountsFound) {
  proto_db_->MockLoadAllResponse(true, false, false);

  {
    InSequence s;
    EXPECT_CALL(*proto_db_, InsertContent(kDiscountsUrlFromServer,
                                          ExpectedServerProto(), _));
    EXPECT_CALL(*proto_db_, LoadAllEntries);
    EXPECT_CALL(*proto_db_, InsertContent).Times(0);
  }

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  storage_->HandleServerDiscounts(
      std::vector<std::string>{kDiscountsUrlToCheckInDb}, MockServerResults(),
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());
            auto discounts = map.find(GURL(kDiscountsUrlFromServer))->second;
            ASSERT_EQ(1, (int)discounts.size());
            ASSERT_EQ(kDiscountIdFromServer, discounts[0].id);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 3, 1);
}

TEST_F(DiscountsStorageTest,
       TestHandleServerDiscounts_URLWithDiscountUTM_WithMatching) {
  proto_db_->MockLoadAllResponse(true, false, false);
  std::string url_with_utm(kDiscountsUrlInDb);
  url_with_utm = url_with_utm +
                 "?utm_source=chrome&utm_medium=app&utm_campaign=chrome-"
                 "history-cluster-with-discount";

  EXPECT_CALL(*proto_db_, InsertContent).Times(0);
  EXPECT_CALL(*proto_db_, LoadAllEntries);

  base::RunLoop run_loop;
  storage_->HandleServerDiscounts(
      std::vector<std::string>{url_with_utm}, {},
      base::BindOnce(
          [](base::RunLoop* run_loop, std::string url_with_utm,
             const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());
            auto discounts = map.find(GURL(url_with_utm))->second;
            ASSERT_EQ(2, (int)discounts.size());
            ASSERT_EQ(kDiscountIdInDb1, discounts[0].id);
            ASSERT_EQ(kDiscountIdInDb2, discounts[1].id);
            run_loop->Quit();
          },
          &run_loop, url_with_utm));
  run_loop.Run();
}

TEST_F(DiscountsStorageTest,
       TestHandleServerDiscounts_URLWithDiscountUTM_WithoutMatching) {
  proto_db_->MockLoadAllResponse(true, false, false);
  std::string url_with_wrong_utm(kDiscountsUrlInDb);
  url_with_wrong_utm =
      url_with_wrong_utm +
      "?utm_source=test&utm_medium=app&utm_campaign=ramdom-campaign";
  std::string wrong_url_with_utm(
      "http://example.com/"
      "discounts_wrong?utm_source=test&utm_medium=app&utm_campaign=ramdom-"
      "campaign");

  base::RunLoop run_loop[2];
  storage_->HandleServerDiscounts(
      std::vector<std::string>{url_with_wrong_utm}, {},
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(0, (int)map.size());
            run_loop->Quit();
          },
          &run_loop[0]));
  run_loop[0].Run();

  storage_->HandleServerDiscounts(
      std::vector<std::string>{wrong_url_with_utm}, {},
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(0, (int)map.size());
            run_loop->Quit();
          },
          &run_loop[1]));
  run_loop[1].Run();
}

TEST_F(DiscountsStorageTest, TestOnURLsDeleted_DeleteAll) {
  EXPECT_CALL(*proto_db_, DeleteAllContent).Times(1);
  EXPECT_CALL(*proto_db_, DeleteOneEntry).Times(0);

  storage_->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());
}

TEST_F(DiscountsStorageTest, TestOnURLsDeleted_DeleteUrls) {
  EXPECT_CALL(*proto_db_, DeleteAllContent).Times(0);
  EXPECT_CALL(*proto_db_, DeleteOneEntry(kDeleteUrl1, _)).Times(1);
  EXPECT_CALL(*proto_db_, DeleteOneEntry(kDeleteUrl2, _)).Times(1);

  storage_->OnURLsDeleted(nullptr, history::DeletionInfo::ForUrls(
                                       {history::URLRow(GURL(kDeleteUrl1)),
                                        history::URLRow(GURL(kDeleteUrl2))},
                                       {}));
}

}  // namespace commerce
