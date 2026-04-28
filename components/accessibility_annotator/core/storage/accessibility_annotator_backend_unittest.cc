// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/icu_test_util.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/types/optional_ref.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/accessibility_annotator/core/storage/test_accessibility_annotator_backend.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {
namespace {

using ::base::test::DictionaryHasValues;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Property;

MATCHER_P(EqualsAnnotations, expected_ref, "") {
  const AccessibilityAnnotatorBackend::ContentAnnotationsData& expected =
      expected_ref.get();

  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field("page_title",
                         &AccessibilityAnnotatorBackend::
                             ContentAnnotationsData::page_title,
                         expected.page_title),
          testing::Field(
              "tab_id",
              &AccessibilityAnnotatorBackend::ContentAnnotationsData::tab_id,
              expected.tab_id),
          testing::Field("content_annotation",
                         &AccessibilityAnnotatorBackend::
                             ContentAnnotationsData::content_annotation,
                         base::test::EqualsProto(expected.content_annotation)),
          testing::Field("classifier_results",
                         &AccessibilityAnnotatorBackend::
                             ContentAnnotationsData::classifier_results,
                         testing::Eq(std::ref(expected.classifier_results))),
          testing::Field("navigation_timestamp",
                         &AccessibilityAnnotatorBackend::
                             ContentAnnotationsData::navigation_timestamp,
                         expected.navigation_timestamp),
          testing::Field(
              "url",
              &AccessibilityAnnotatorBackend::ContentAnnotationsData::url,
              expected.url)),
      arg, result_listener);
}

constexpr std::string_view kExampleUrl = "https://example.com/";

base::Time GetTimeForTest() {
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString("2026-04-10 10:00:00 UTC", &now));
  return now;
}

AccessibilityAnnotatorBackend::ContentAnnotationsData
CreateContentAnnotationsData(std::string_view page_title) {
  AccessibilityAnnotatorBackend::ContentAnnotationsData data;
  data.page_title = std::string(page_title);
  data.navigation_timestamp = GetTimeForTest();
  data.url = GURL(kExampleUrl);
  data.tab_id = 123;
  return data;
}

class AccessibilityAnnotatorBackendTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
    backend_ = std::make_unique<AccessibilityAnnotatorBackendImpl>(
        /*history_service=*/nullptr, os_crypt_async_.get(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        temp_dir_.GetPath().AppendASCII("TestDB"));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityAnnotatorDatabaseStorage};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::unique_ptr<AccessibilityAnnotatorBackendImpl> backend_;
};

class AccessibilityAnnotatorBackendNoInitTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityAnnotatorDatabaseStorage};
  base::ScopedTempDir temp_dir_;
};

TEST_F(AccessibilityAnnotatorBackendTest, GetContentAnnotationsCacheData) {
  history::VisitID visit_id(123);
  std::string page_title = "Test Page Title";
  optimization_guide::proto::ContentAnnotation content_annotation;
  content_annotation.set_description("Test description");
  content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  content_annotation.mutable_structured_data()->add_orders()->set_id(
      "order_123");
  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "test_category");

  // Cache should be empty initially.
  ASSERT_FALSE(backend_->GetContentAnnotationsCacheData(visit_id).has_value());

  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.content_annotation = std::move(content_annotation);
  data.classifier_results = classifier_results.Clone();
  backend_->SetContentAnnotationsCacheData(visit_id, std::move(data));

  base::optional_ref<
      const AccessibilityAnnotatorBackend::ContentAnnotationsData>
      cached_data = backend_->GetContentAnnotationsCacheData(visit_id);
  ASSERT_TRUE(cached_data.has_value());
  EXPECT_EQ(cached_data->page_title, page_title);
  EXPECT_EQ(cached_data->url, GURL(kExampleUrl));
  ASSERT_TRUE(cached_data->tab_id.has_value());
  EXPECT_EQ(*cached_data->tab_id, 123);
  EXPECT_EQ(cached_data->content_annotation.description(), "Test description");
  EXPECT_EQ(cached_data->content_annotation.status(),
            optimization_guide::proto::ContentAnnotation::CONFIRMED);
  ASSERT_EQ(cached_data->content_annotation.structured_data().orders_size(), 1);
  EXPECT_EQ(cached_data->content_annotation.structured_data().orders(0).id(),
            "order_123");
  EXPECT_EQ(cached_data->classifier_results, classifier_results);
  EXPECT_EQ(cached_data->navigation_timestamp, GetTimeForTest());
}

TEST_F(AccessibilityAnnotatorBackendTest, GetAnnotationsForDebugUIEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAccessibilityAnnotatorDatabaseStorage);

  base::test::TestFuture<base::Value> future;
  backend_->GetAnnotationsForDebugUI(future.GetCallback());
  base::Value result = future.Take();

  ASSERT_TRUE(result.is_list());
  EXPECT_TRUE(result.GetList().empty());
}

TEST_F(AccessibilityAnnotatorBackendTest, GetAnnotationsForDebugUIFromCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAccessibilityAnnotatorDatabaseStorage);

  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  base::test::ScopedRestoreDefaultTimezone timezone("UTC");
  history::VisitID visit_id(123);
  std::string page_title = "Test Page Title";
  optimization_guide::proto::ContentAnnotation content_annotation;
  content_annotation.set_description("Test annotation description");
  content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* order = content_annotation.mutable_structured_data()->add_orders();
  order->set_id("order_123");

  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "test category");
  base::DictValue expected_classifier = classifier_results.Clone();

  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.classifier_results = std::move(classifier_results);
  data.content_annotation = std::move(content_annotation);
  backend_->SetContentAnnotationsCacheData(visit_id, std::move(data));

  base::test::TestFuture<base::Value> future;
  backend_->GetAnnotationsForDebugUI(future.GetCallback());
  base::Value result = future.Take();

  ASSERT_TRUE(result.is_list());
  const base::ListValue& list = result.GetList();
  ASSERT_EQ(list.size(), 1u);

  const base::DictValue& entry = list[0].GetDict();
  EXPECT_THAT(
      entry,
      DictionaryHasValues(
          base::DictValue()
              .Set("url", GURL(kExampleUrl).spec())
              .Set("title", page_title)
              .Set("tab_id", 123)
              .Set("visit_id", "123")
              .Set("navigation_timestamp",
                   "4/10/26, 10:00:00\xe2\x80\xaf"
                   "AM")
              .Set("classifier_results", std::move(expected_classifier))
              .Set("content_annotation",
                   base::DictValue()
                       .Set("description", "Test annotation description")
                       .Set("status", "CONFIRMED")
                       .Set("structured_data",
                            base::DictValue().Set(
                                "orders",
                                base::ListValue().Append(base::DictValue().Set(
                                    "id", "order_123")))))));
}

TEST_F(AccessibilityAnnotatorBackendTest, GetAnnotationsForDebugUIFromDb) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");
  base::test::ScopedRestoreDefaultTimezone timezone("UTC");
  history::VisitID visit_id(1);
  std::string page_title = "DB Page Title";

  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "db category");
  base::DictValue expected_classifier = classifier_results.Clone();

  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.classifier_results = std::move(classifier_results);
  data.content_annotation.set_description("Test description");
  data.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  data.content_annotation.mutable_structured_data()->add_orders()->set_id(
      "order_456");

  // Add to the database.
  base::test::TestFuture<bool> add_future;
  backend_->AddContentAnnotation(visit_id, std::move(data),
                                 add_future.GetCallback());
  ASSERT_TRUE(add_future.Get());

  // Get annotations from the database for the debug UI and verify the result.
  base::test::TestFuture<base::Value> annotations_future;
  backend_->GetAnnotationsForDebugUI(annotations_future.GetCallback());
  base::Value result = annotations_future.Take();

  ASSERT_TRUE(result.is_list());
  const base::ListValue& list = result.GetList();
  ASSERT_EQ(list.size(), 1u);

  EXPECT_THAT(
      list[0].GetDict(),
      DictionaryHasValues(
          base::DictValue()
              .Set("url", GURL(kExampleUrl).spec())
              .Set("title", page_title)
              .Set("tab_id", 123)
              .Set("visit_id", "1")
              .Set("navigation_timestamp",
                   "4/10/26, 10:00:00\xe2\x80\xaf"
                   "AM")
              .Set("classifier_results", std::move(expected_classifier))
              .Set("content_annotation",
                   base::DictValue()
                       .Set("description", "Test description")
                       .Set("status", "CONFIRMED")
                       .Set("structured_data",
                            base::DictValue().Set(
                                "orders",
                                base::ListValue().Append(base::DictValue().Set(
                                    "id", "order_456")))))));
}

TEST_F(AccessibilityAnnotatorBackendTest, SetContentAnnotationsCacheData) {
  history::VisitID visit_id(123);
  std::string page_title = "Test Page Title";
  optimization_guide::proto::ContentAnnotation content_annotation;
  content_annotation.set_description("Only proto description");
  content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  content_annotation.mutable_structured_data()->add_orders()->set_id(
      "order_123");

  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.content_annotation = std::move(content_annotation);
  backend_->SetContentAnnotationsCacheData(visit_id, std::move(data));

  base::optional_ref<
      const AccessibilityAnnotatorBackend::ContentAnnotationsData>
      cached_data = backend_->GetContentAnnotationsCacheData(visit_id);
  ASSERT_TRUE(cached_data.has_value());
  EXPECT_EQ(cached_data->page_title, page_title);
  EXPECT_EQ(cached_data->url, GURL(kExampleUrl));
  EXPECT_EQ(cached_data->content_annotation.description(),
            "Only proto description");
  EXPECT_EQ(cached_data->content_annotation.status(),
            optimization_guide::proto::ContentAnnotation::CONFIRMED);
  ASSERT_EQ(cached_data->content_annotation.structured_data().orders_size(), 1);
  EXPECT_EQ(cached_data->content_annotation.structured_data().orders(0).id(),
            "order_123");
  EXPECT_EQ(cached_data->navigation_timestamp, GetTimeForTest());
}

TEST_F(AccessibilityAnnotatorBackendTest, RemoveContentAnnotationsCacheData) {
  history::VisitID visit_id1(1);
  history::VisitID visit_id2(2);
  history::VisitID visit_id3(3);

  backend_->SetContentAnnotationsCacheData(
      visit_id1, CreateContentAnnotationsData("Page 1"));
  backend_->SetContentAnnotationsCacheData(
      visit_id2, CreateContentAnnotationsData("Page 2"));
  backend_->SetContentAnnotationsCacheData(
      visit_id3, CreateContentAnnotationsData("Page 3"));

  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(visit_id1).has_value());
  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(visit_id2).has_value());
  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(visit_id3).has_value());

  backend_->RemoveContentAnnotationsCacheData({visit_id1, visit_id2});
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(visit_id1).has_value());
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(visit_id2).has_value());
  EXPECT_TRUE(backend_->GetContentAnnotationsCacheData(visit_id3).has_value());

  backend_->RemoveContentAnnotationsCacheData({visit_id3});
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(visit_id3).has_value());
}

TEST_F(AccessibilityAnnotatorBackendTest, ClearContentAnnotationsCache) {
  history::VisitID visit_id1(1);
  history::VisitID visit_id2(2);

  backend_->SetContentAnnotationsCacheData(
      visit_id1, CreateContentAnnotationsData("Page 1"));
  backend_->SetContentAnnotationsCacheData(
      visit_id2, CreateContentAnnotationsData("Page 2"));

  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(visit_id1).has_value());
  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(visit_id2).has_value());

  backend_->ClearContentAnnotationsCache();
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(visit_id1).has_value());
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(visit_id2).has_value());
}

TEST_F(AccessibilityAnnotatorBackendTest, ObserverNotified) {
  MockAccessibilityAnnotatorBackendObserver observer;
  backend_->AddObserver(&observer);

  history::VisitID visit_id(123);
  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData("Test Page Title");
  data.content_annotation.set_description("Test Description");

  EXPECT_CALL(observer,
              OnContentAnnotationsAdded(
                  Eq(visit_id),
                  AllOf(
                      // Match the plain struct member
                      Field(&AccessibilityAnnotatorBackend::
                                ContentAnnotationsData::page_title,
                            Eq("Test Page Title")),

                      // Match the proto field
                      Field(&AccessibilityAnnotatorBackend::
                                ContentAnnotationsData::content_annotation,
                            Property(&optimization_guide::proto::
                                         ContentAnnotation::description,
                                     Eq("Test Description"))))));
  backend_->SetContentAnnotationsCacheData(visit_id, std::move(data));
  testing::Mock::VerifyAndClearExpectations(&observer);

  backend_->RemoveObserver(&observer);
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_TimestampSorting) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  AccessibilityAnnotatorBackend::ContentAnnotationsData data1;
  data1.tab_id = 123;
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  optimization_guide::proto::ContentAnnotation ca1;
  ca1.set_description("Older");
  ca1.set_status(optimization_guide::proto::ContentAnnotation::CONFIRMED);
  data1.content_annotation = ca1;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  AccessibilityAnnotatorBackend::ContentAnnotationsData data2;
  data2.tab_id = 123;
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  optimization_guide::proto::ContentAnnotation ca2;
  ca2.set_description("Newer");
  ca2.set_status(optimization_guide::proto::ContentAnnotation::CONFIRMED);
  data2.content_annotation = ca2;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 2u);
  EXPECT_EQ(merged[0].description(), "Older");
  EXPECT_EQ(merged[1].description(), "Newer");
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_EtldPlusOneMatching) {
  GURL url1("https://example.com/1");
  GURL url2("https://different.com/2");

  AccessibilityAnnotatorBackend::ContentAnnotationsData data1;
  data1.tab_id = 123;
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  optimization_guide::proto::ContentAnnotation ca1;
  ca1.set_description("Other Domain");
  data1.content_annotation = ca1;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  AccessibilityAnnotatorBackend::ContentAnnotationsData data2;
  data2.tab_id = 123;
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  optimization_guide::proto::ContentAnnotation ca2;
  ca2.set_description("Target Domain");
  ca2.set_status(optimization_guide::proto::ContentAnnotation::CONFIRMED);
  data2.content_annotation = ca2;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  EXPECT_EQ(merged[0].description(), "Target Domain");
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_IgnoresNewerEntries) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Add a newer entry that is PENDING.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2;
  data2.tab_id = 123;
  data2.navigation_timestamp = base::Time::Now() + base::Minutes(1);
  data2.url = url2;
  optimization_guide::proto::ContentAnnotation ca2;
  ca2.set_description("Newer Pending");
  data2.content_annotation = ca2;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Add an older entry that becomes CONFIRMED (this triggers lookback).
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1;
  data1.tab_id = 123;
  data1.navigation_timestamp = base::Time::Now();
  data1.url = url1;
  optimization_guide::proto::ContentAnnotation ca1;
  ca1.set_description("Older Confirmed");
  ca1.set_status(optimization_guide::proto::ContentAnnotation::CONFIRMED);
  data1.content_annotation = ca1;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  EXPECT_EQ(merged[0].description(), "Older Confirmed");
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_TimeRangeCheck) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  AccessibilityAnnotatorBackend::ContentAnnotationsData data1;
  data1.tab_id = 123;
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(25);
  data1.url = url1;
  optimization_guide::proto::ContentAnnotation ca1;
  ca1.set_description("Too Old");
  data1.content_annotation = ca1;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  AccessibilityAnnotatorBackend::ContentAnnotationsData data2;
  data2.tab_id = 123;
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  optimization_guide::proto::ContentAnnotation ca2;
  ca2.set_description("Newer");
  ca2.set_status(optimization_guide::proto::ContentAnnotation::CONFIRMED);
  data2.content_annotation = ca2;
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  EXPECT_EQ(merged[0].description(), "Newer");
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_MaxSizeEnforced) {
  int max_size = features::kContentAnnotatorMaxCacheAnnotations.Get();
  for (int i = 0; i < max_size + 10; ++i) {
    GURL url("https://example.com/" + base::NumberToString(i));
    AccessibilityAnnotatorBackend::ContentAnnotationsData data;
    data.tab_id = 123;
    data.navigation_timestamp = base::Time::Now();
    data.url = url;
    optimization_guide::proto::ContentAnnotation ca;
    ca.set_status(optimization_guide::proto::ContentAnnotation::CONFIRMED);
    data.content_annotation = ca;
    backend_->SetContentAnnotationsCacheData(
        static_cast<history::VisitID>(i + 1), std::move(data));
  }

  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  EXPECT_EQ(merged.size(), static_cast<size_t>(max_size));
}

TEST_F(AccessibilityAnnotatorBackendTest, OnContentAnnotationsDeletedNotified) {
  MockAccessibilityAnnotatorBackendObserver observer;
  backend_->AddObserver(&observer);

  history::VisitID visit_id1(1);
  history::VisitID visit_id2(2);

  backend_->SetContentAnnotationsCacheData(
      visit_id1, CreateContentAnnotationsData("Page 1"));
  backend_->SetContentAnnotationsCacheData(
      visit_id2, CreateContentAnnotationsData("Page 2"));

  EXPECT_CALL(observer,
              OnContentAnnotationsDeleted(testing::ElementsAre(visit_id1)));
  backend_->RemoveContentAnnotationsCacheData({visit_id1});
  testing::Mock::VerifyAndClearExpectations(&observer);

  backend_->RemoveObserver(&observer);
}

TEST_F(AccessibilityAnnotatorBackendTest, OnContentAnnotationsClearedNotified) {
  MockAccessibilityAnnotatorBackendObserver observer;
  backend_->AddObserver(&observer);

  // Empty cache should not notify the observer.
  EXPECT_CALL(observer, OnContentAnnotationsCleared()).Times(0);
  backend_->ClearContentAnnotationsCache();
  testing::Mock::VerifyAndClearExpectations(&observer);

  history::VisitID visit_id1(1);
  history::VisitID visit_id2(2);

  backend_->SetContentAnnotationsCacheData(
      visit_id1, CreateContentAnnotationsData("Page 1"));
  backend_->SetContentAnnotationsCacheData(
      visit_id2, CreateContentAnnotationsData("Page 2"));

  EXPECT_CALL(observer, OnContentAnnotationsCleared());
  backend_->ClearContentAnnotationsCache();
  testing::Mock::VerifyAndClearExpectations(&observer);

  backend_->RemoveObserver(&observer);
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_MergeStructuredData_Order) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Set up an older pending entry with an order containing products.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("");
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  auto* order1 =
      data1.content_annotation.mutable_structured_data()->add_orders();
  order1->set_id("order_123");
  order1->set_grand_total(100.0);

  auto* p1 = order1->add_products();
  p1->set_name("Product A");
  p1->set_quantity(2);
  auto* p2 = order1->add_products();
  p2->set_name("Product B");
  p2->set_quantity(1);
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  // Set up a newer confirmed entry with the same order ID but missing products
  // and grand total.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("");
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  data2.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* order2 =
      data2.content_annotation.mutable_structured_data()->add_orders();
  order2->set_id("order_123");
  auto* date = order2->mutable_order_date();
  date->set_year(2026);
  date->set_month(4);
  date->set_day(10);
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Verify that the lookback mechanism correctly merged the data from both
  // entries.
  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  const auto& sd = merged[0].structured_data();
  ASSERT_EQ(sd.orders_size(), 1);
  const auto& o = sd.orders(0);
  EXPECT_EQ(o.id(), "order_123");
  EXPECT_EQ(o.grand_total(), 100.0);
  EXPECT_EQ(o.order_date().year(), 2026);
  EXPECT_EQ(o.order_date().month(), 4);
  EXPECT_EQ(o.order_date().day(), 10);
  ASSERT_EQ(o.products_size(), 2);
  EXPECT_EQ(o.products(0).name(), "Product A");
  EXPECT_EQ(o.products(0).quantity(), 2);
  EXPECT_EQ(o.products(1).name(), "Product B");
  EXPECT_EQ(o.products(1).quantity(), 1);
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_MergeStructuredData_Order_Conflict) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Set up an older pending entry with an order and a specific grand total.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("");
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  auto* order1 =
      data1.content_annotation.mutable_structured_data()->add_orders();
  order1->set_id("order_123");
  order1->set_grand_total(100.0);
  auto* date1 = order1->mutable_order_date();
  date1->set_year(2026);
  date1->set_month(4);
  date1->set_day(9);
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  // Set up a newer confirmed entry with a conflicting grand total.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("");
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  data2.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* order2 =
      data2.content_annotation.mutable_structured_data()->add_orders();
  order2->set_id("order_123");
  order2->set_grand_total(200.0);
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Verify that the entries were not merged due to the conflict in grand total.
  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  const auto& sd = merged[0].structured_data();
  ASSERT_EQ(sd.orders_size(), 1);
  const auto& o = sd.orders(0);
  EXPECT_EQ(o.id(), "order_123");
  EXPECT_EQ(o.grand_total(), 200.0);
  EXPECT_FALSE(o.has_order_date());
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_MergeStructuredData_Shipment) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Set up an older pending entry with a shipment containing a carrier name.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("");
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  auto* shipment1 =
      data1.content_annotation.mutable_structured_data()->add_shipments();
  shipment1->set_tracking_number("track_123");
  shipment1->set_carrier_name("UPS");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  // Set up a newer confirmed entry with the same tracking number and a delivery
  // address.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("");
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  data2.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* shipment2 =
      data2.content_annotation.mutable_structured_data()->add_shipments();
  shipment2->set_tracking_number("track_123");
  shipment2->set_delivery_address("123 Main St");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Verify that the lookback mechanism correctly merged the shipment data.
  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  const auto& sd = merged[0].structured_data();
  ASSERT_EQ(sd.shipments_size(), 1);
  const auto& s = sd.shipments(0);
  EXPECT_EQ(s.tracking_number(), "track_123");
  EXPECT_EQ(s.delivery_address(), "123 Main St");
  EXPECT_EQ(s.carrier_name(), "UPS");
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_MergeStructuredData_Shipment_Conflict) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Set up an older pending entry with a shipment containing a carrier name and
  // delivery address.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("");
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  auto* shipment1 =
      data1.content_annotation.mutable_structured_data()->add_shipments();
  shipment1->set_tracking_number("track_123");
  shipment1->set_carrier_name("UPS");
  shipment1->set_delivery_address("456 Side St");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  // Set up a newer confirmed entry with a conflicting carrier name for the same
  // tracking number.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("");
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  data2.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* shipment2 =
      data2.content_annotation.mutable_structured_data()->add_shipments();
  shipment2->set_tracking_number("track_123");
  shipment2->set_carrier_name("FedEx");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Verify that the entries were not merged due to the conflict in carrier
  // name.
  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  ASSERT_TRUE(merged[0].has_structured_data());
  const auto& sd = merged[0].structured_data();
  ASSERT_EQ(sd.shipments_size(), 1);
  const auto& s123 = sd.shipments(0);
  EXPECT_EQ(s123.tracking_number(), "track_123");
  EXPECT_EQ(s123.carrier_name(), "FedEx");
  EXPECT_FALSE(s123.has_delivery_address());
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_MergeStructuredData_FlightReservation) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Set up an older pending entry with a flight reservation containing a flight
  // number.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("");
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  auto* flight1 = data1.content_annotation.mutable_structured_data()
                      ->add_flight_reservations();
  flight1->set_confirmation_code("flight_123");
  flight1->set_flight_number("AA123");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  // Set up a newer confirmed entry with the same confirmation code but a
  // passenger name.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("");
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  data2.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* flight2 = data2.content_annotation.mutable_structured_data()
                      ->add_flight_reservations();
  flight2->set_confirmation_code("flight_123");
  flight2->set_passenger_name("John Doe");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Verify that the lookback mechanism correctly merged the flight reservation
  // data.
  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  const auto& sd = merged[0].structured_data();
  ASSERT_EQ(sd.flight_reservations_size(), 1);
  const auto& f = sd.flight_reservations(0);
  EXPECT_EQ(f.confirmation_code(), "flight_123");
  EXPECT_EQ(f.flight_number(), "AA123");
  EXPECT_EQ(f.passenger_name(), "John Doe");
}

TEST_F(
    AccessibilityAnnotatorBackendTest,
    ProcessConfirmedStatusLookback_MergeStructuredData_FlightReservation_Conflict) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Set up an older pending entry with a flight reservation containing a flight
  // number.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("");
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(10);
  data1.url = url1;
  auto* flight1 = data1.content_annotation.mutable_structured_data()
                      ->add_flight_reservations();
  flight1->set_confirmation_code("flight_123");
  flight1->set_flight_number("AA123");
  flight1->set_passenger_name("John Doe");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  // Set up a newer confirmed entry with a conflicting flight number.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("");
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url2;
  data2.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* flight2 = data2.content_annotation.mutable_structured_data()
                      ->add_flight_reservations();
  flight2->set_confirmation_code("flight_123");
  flight2->set_flight_number("UA456");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Verify that the entries were not merged due to the conflict in flight
  // number.
  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  const auto& sd = merged[0].structured_data();
  ASSERT_EQ(sd.flight_reservations_size(), 1);
  const auto& f = sd.flight_reservations(0);
  EXPECT_EQ(f.confirmation_code(), "flight_123");
  EXPECT_EQ(f.flight_number(), "UA456");
  EXPECT_FALSE(f.has_passenger_name());
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ProcessConfirmedStatusLookback_MergeStructuredData_MultipleTypes) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  // Set up an older pending entry with a flight reservation.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("");
  data1.navigation_timestamp = base::Time::Now() - base::Minutes(1);
  data1.url = url1;
  auto* flight1 = data1.content_annotation.mutable_structured_data()
                      ->add_flight_reservations();
  flight1->set_confirmation_code("flight_123");
  flight1->set_flight_number("AA123");
  flight1->set_departure_airport("SFO");
  flight1->set_arrival_airport("LAX");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(1),
                                           std::move(data1));

  // Set up a newer confirmed entry with an order, shipment, and the same
  // flight reservation.
  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("");
  data2.navigation_timestamp = base::Time::Now();
  data2.url = url1;
  data2.content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* order =
      data2.content_annotation.mutable_structured_data()->add_orders();
  order->set_id("order_123");
  auto* shipment =
      data2.content_annotation.mutable_structured_data()->add_shipments();
  shipment->set_tracking_number("track_123");
  auto* flight2 = data2.content_annotation.mutable_structured_data()
                      ->add_flight_reservations();
  flight2->set_confirmation_code("flight_123");
  flight2->set_flight_number("AA123");
  backend_->SetContentAnnotationsCacheData(static_cast<history::VisitID>(2),
                                           std::move(data2));

  // Verify that all structured data types were correctly merged.
  const auto& merged = backend_->GetMergedMultipageAnnotationsForTesting();
  ASSERT_EQ(merged.size(), 1u);
  const auto& sd = merged[0].structured_data();

  ASSERT_EQ(sd.orders_size(), 1);
  EXPECT_EQ(sd.orders(0).id(), "order_123");

  ASSERT_EQ(sd.shipments_size(), 1);
  EXPECT_EQ(sd.shipments(0).tracking_number(), "track_123");

  ASSERT_EQ(sd.flight_reservations_size(), 1);
  EXPECT_EQ(sd.flight_reservations(0).confirmation_code(), "flight_123");
  EXPECT_EQ(sd.flight_reservations(0).flight_number(), "AA123");
  EXPECT_EQ(sd.flight_reservations(0).departure_airport(), "SFO");
  EXPECT_EQ(sd.flight_reservations(0).arrival_airport(), "LAX");
}

TEST_F(AccessibilityAnnotatorBackendTest, AddAndGetContentAnnotationFromDb) {
  history::VisitID visit_id(123);
  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData("Test Page Title");
  data.content_annotation.set_description("Test description");
  data.classifier_results.Set("category", "test_category");

  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<
      std::optional<AccessibilityAnnotatorBackend::ContentAnnotationsData>>
      get_future;

  // Add the content annotation to the database.
  backend_->AddContentAnnotation(visit_id, data.Clone(),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  // Retrieve the content annotation from the database and verify its contents.
  backend_->GetContentAnnotation(visit_id, get_future.GetCallback());
  EXPECT_THAT(get_future.Get(), Optional(EqualsAnnotations(std::ref(data))));
}

TEST_F(AccessibilityAnnotatorBackendTest,
       GetNonExistentContentAnnotationFromDb) {
  base::test::TestFuture<
      std::optional<AccessibilityAnnotatorBackend::ContentAnnotationsData>>
      future;
  backend_->GetContentAnnotation(999, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(AccessibilityAnnotatorBackendTest, GetAllContentAnnotationsFromDb) {
  history::VisitID visit_id1(1);
  history::VisitID visit_id2(2);
  AccessibilityAnnotatorBackend::ContentAnnotationsData data1 =
      CreateContentAnnotationsData("Test Page Title 1");
  data1.content_annotation.set_description("Test description 1");
  data1.classifier_results.Set("category", "test_category");

  AccessibilityAnnotatorBackend::ContentAnnotationsData data2 =
      CreateContentAnnotationsData("Test Page Title 2");
  data2.content_annotation.set_description("Test description 2");
  data2.classifier_results.Set("category", "test_category");

  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<std::vector<std::pair<
      history::VisitID, AccessibilityAnnotatorBackend::ContentAnnotationsData>>>
      get_future;

  // Add two content annotations to the database.
  backend_->AddContentAnnotation(visit_id1, data1.Clone(),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  backend_->AddContentAnnotation(visit_id2, data2.Clone(),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  // Verify that both content annotations are retrieved from the database.
  backend_->GetAllContentAnnotations(get_future.GetCallback());
  EXPECT_THAT(get_future.Get(),
              testing::UnorderedElementsAre(
                  Pair(visit_id1, EqualsAnnotations(std::ref(data1))),
                  Pair(visit_id2, EqualsAnnotations(std::ref(data2)))));
}

TEST_F(AccessibilityAnnotatorBackendTest, ClearAllContentAnnotationsFromDb) {
  history::VisitID visit_id1(1);
  history::VisitID visit_id2(2);
  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData("Test Page Title");
  data.content_annotation.set_description("Test description");
  data.classifier_results.Set("category", "test_category");

  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<std::vector<std::pair<
      history::VisitID, AccessibilityAnnotatorBackend::ContentAnnotationsData>>>
      get_future;

  // Add two content annotations to the database.
  backend_->AddContentAnnotation(visit_id1, data.Clone(),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  backend_->AddContentAnnotation(visit_id2, data.Clone(),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  // Verify that the database is not empty.
  backend_->GetAllContentAnnotations(get_future.GetCallback());
  ASSERT_EQ(get_future.Take().size(), 2u);

  // Clear all content annotations from the database successfully.
  backend_->ClearAllContentAnnotations(success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  // Verify that the database is empty after clearing all content annotations.
  backend_->GetAllContentAnnotations(get_future.GetCallback());
  EXPECT_TRUE(get_future.Take().empty());
}

TEST_F(AccessibilityAnnotatorBackendTest, DeleteContentAnnotationsFromDb) {
  history::VisitID visit_id_1(1);
  history::VisitID visit_id_2(2);
  history::VisitID visit_id_3(3);

  base::test::TestFuture<bool> success_future;
  base::test::TestFuture<std::vector<std::pair<
      history::VisitID, AccessibilityAnnotatorBackend::ContentAnnotationsData>>>
      get_future;

  // Add the content annotations to the database.
  backend_->AddContentAnnotation(visit_id_1,
                                 CreateContentAnnotationsData("Title 1"),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  backend_->AddContentAnnotation(visit_id_2,
                                 CreateContentAnnotationsData("Title 2"),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());
  backend_->AddContentAnnotation(visit_id_3,
                                 CreateContentAnnotationsData("Title 3"),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  // Verify that the content annotations are present in the database.
  backend_->GetAllContentAnnotations(get_future.GetCallback());
  EXPECT_EQ(get_future.Take().size(), 3u);

  // Delete two content annotations from the database successfully.
  backend_->DeleteContentAnnotations({visit_id_1, visit_id_2},
                                     success_future.GetCallback());
  EXPECT_TRUE(success_future.Take());

  // Verify that only the remaining content annotation is present in the
  // database.
  backend_->GetAllContentAnnotations(get_future.GetCallback());
  EXPECT_THAT(get_future.Take(),
              testing::ElementsAre(Pair(visit_id_3, testing::_)));
}

TEST_F(AccessibilityAnnotatorBackendTest,
       AddContentAnnotationToDbNotifiesObserver) {
  MockAccessibilityAnnotatorBackendObserver observer;
  backend_->AddObserver(&observer);

  history::VisitID visit_id(123);
  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData("Test Page Title");
  data.content_annotation.set_description("Test description");
  data.classifier_results.Set("category", "test_category");

  EXPECT_CALL(
      observer,
      OnContentAnnotationsAdded(
          Eq(visit_id),
          AllOf(
              Field(&AccessibilityAnnotatorBackend::ContentAnnotationsData::
                        page_title,
                    Eq("Test Page Title")),
              Field(&AccessibilityAnnotatorBackend::ContentAnnotationsData::
                        content_annotation,
                    Property(&optimization_guide::proto::ContentAnnotation::
                                 description,
                             Eq("Test description"))),
              Field(&AccessibilityAnnotatorBackend::ContentAnnotationsData::
                        classifier_results,
                    base::test::IsJson("{\"category\":\"test_category\"}")))));

  base::test::TestFuture<bool> success_future;
  backend_->AddContentAnnotation(visit_id, std::move(data),
                                 success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  testing::Mock::VerifyAndClearExpectations(&observer);
  backend_->RemoveObserver(&observer);
}

TEST_F(AccessibilityAnnotatorBackendTest,
       DeleteContentAnnotationFromDbNotifiesObserver) {
  history::VisitID visit_id(123);
  base::test::TestFuture<bool> add_future;
  backend_->AddContentAnnotation(visit_id,
                                 CreateContentAnnotationsData("Title"),
                                 add_future.GetCallback());
  ASSERT_TRUE(add_future.Get());

  MockAccessibilityAnnotatorBackendObserver observer;
  backend_->AddObserver(&observer);

  // Only visit_id should be in the notification.
  EXPECT_CALL(observer,
              OnContentAnnotationsDeleted(testing::ElementsAre(visit_id)));

  base::test::TestFuture<bool> success_future;
  // Attempt to delete content annotations from the database, including a
  // visit ID that doesn't exist.
  backend_->DeleteContentAnnotations({visit_id, 999},
                                     success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  testing::Mock::VerifyAndClearExpectations(&observer);
  backend_->RemoveObserver(&observer);
}

TEST_F(AccessibilityAnnotatorBackendTest,
       ClearAllContentAnnotationsFromDbNotifiesObserver) {
  MockAccessibilityAnnotatorBackendObserver observer;
  backend_->AddObserver(&observer);

  base::test::TestFuture<bool> add_future;
  backend_->AddContentAnnotation(123, CreateContentAnnotationsData("Title"),
                                 add_future.GetCallback());
  ASSERT_TRUE(add_future.Get());

  EXPECT_CALL(observer, OnContentAnnotationsCleared());

  base::test::TestFuture<bool> success_future;
  backend_->ClearAllContentAnnotations(success_future.GetCallback());
  EXPECT_TRUE(success_future.Get());

  testing::Mock::VerifyAndClearExpectations(&observer);
  backend_->RemoveObserver(&observer);
}

// Tests that adding a content annotation doesn't notify observers
// when the database is not initialized.
TEST_F(AccessibilityAnnotatorBackendNoInitTest,
       AddContentAnnotationBeforeInit) {
  std::unique_ptr<AccessibilityAnnotatorBackendImpl> backend =
      std::make_unique<AccessibilityAnnotatorBackendImpl>(
          /*history_service=*/nullptr, /*os_crypt_async=*/nullptr,
          syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
          temp_dir_.GetPath().AppendASCII("AddBeforeInit"));

  MockAccessibilityAnnotatorBackendObserver observer;
  backend->AddObserver(&observer);
  EXPECT_CALL(observer, OnContentAnnotationsAdded).Times(0);

  base::test::TestFuture<bool> future;
  backend->AddContentAnnotation(static_cast<history::VisitID>(1),
                                CreateContentAnnotationsData("Title"),
                                future.GetCallback());
  EXPECT_FALSE(future.Get());

  backend->RemoveObserver(&observer);
}

// Tests that deleting content annotations doesn't notify observers when the
// database is not initialized.
TEST_F(AccessibilityAnnotatorBackendNoInitTest,
       DeleteContentAnnotationsBeforeInit) {
  std::unique_ptr<AccessibilityAnnotatorBackendImpl> backend =
      std::make_unique<AccessibilityAnnotatorBackendImpl>(
          /*history_service=*/nullptr, /*os_crypt_async=*/nullptr,
          syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
          temp_dir_.GetPath().AppendASCII("DeleteBeforeInit"));

  MockAccessibilityAnnotatorBackendObserver observer;
  backend->AddObserver(&observer);
  EXPECT_CALL(observer, OnContentAnnotationsDeleted).Times(0);

  base::test::TestFuture<bool> future;
  backend->DeleteContentAnnotations({static_cast<history::VisitID>(1)},
                                    future.GetCallback());
  // Implementation returns true if the DB is not open but nothing was deleted.
  EXPECT_TRUE(future.Get());

  backend->RemoveObserver(&observer);
}

// Tests that clearing all content annotations doesn't notify
// observers when the database is not initialized.
TEST_F(AccessibilityAnnotatorBackendNoInitTest,
       ClearAllContentAnnotationsBeforeInit) {
  std::unique_ptr<AccessibilityAnnotatorBackendImpl> backend =
      std::make_unique<AccessibilityAnnotatorBackendImpl>(
          /*history_service=*/nullptr, /*os_crypt_async=*/nullptr,
          syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
          temp_dir_.GetPath().AppendASCII("ClearBeforeInit"));

  MockAccessibilityAnnotatorBackendObserver observer;
  backend->AddObserver(&observer);
  EXPECT_CALL(observer, OnContentAnnotationsCleared).Times(0);

  base::test::TestFuture<bool> future;
  backend->ClearAllContentAnnotations(future.GetCallback());
  EXPECT_FALSE(future.Get());

  backend->RemoveObserver(&observer);
}

}  // namespace
}  // namespace accessibility_annotator
