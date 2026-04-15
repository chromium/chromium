// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/types/optional_ref.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/accessibility_annotator/core/storage/test_accessibility_annotator_backend.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
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
using ::testing::Property;

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
  AccessibilityAnnotatorBackendTest() = default;
  ~AccessibilityAnnotatorBackendTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_ = std::make_unique<AccessibilityAnnotatorBackendImpl>(
        /*history_service=*/nullptr,
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        temp_dir_.GetPath().AppendASCII("TestDB"));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AccessibilityAnnotatorBackendImpl> backend_;
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

TEST_F(AccessibilityAnnotatorBackendTest, GetDebugUICacheDataEmpty) {
  base::Value result = backend_->GetDebugUICacheData();
  ASSERT_TRUE(result.is_list());
  EXPECT_TRUE(result.GetList().empty());
}

TEST_F(AccessibilityAnnotatorBackendTest, GetDebugUICacheDataWithEntries) {
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

  base::Value result = backend_->GetDebugUICacheData();
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

}  // namespace
}  // namespace accessibility_annotator
