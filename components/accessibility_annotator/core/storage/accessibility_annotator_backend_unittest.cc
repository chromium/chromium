// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/types/optional_ref.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
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

AccessibilityAnnotatorBackend::ContentAnnotationsData
CreateContentAnnotationsData(std::string_view page_title) {
  AccessibilityAnnotatorBackend::ContentAnnotationsData data;
  data.page_title = std::string(page_title);
  data.navigation_timestamp = base::Time::Now();
  data.visit_id = static_cast<history::VisitID>(123);
  data.tab_id = 123;
  return data;
}

class MockBackendObserver : public AccessibilityAnnotatorBackend::Observer {
 public:
  MOCK_METHOD(void,
              OnContentAnnotationsAdded,
              (const AccessibilityAnnotatorBackend::ContentAnnotationsData&),
              (override));
};

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
  GURL url("https://example.com/");
  std::string page_title = "Test Page Title";
  base::DictValue annotations;
  annotations.Set("1", "value1");
  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "test_category");

  // Cache should be empty initially.
  ASSERT_FALSE(backend_->GetContentAnnotationsCacheData(url).has_value());

  base::Time now = base::Time::Now();
  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.annotations = annotations.Clone();
  data.navigation_timestamp = now;
  data.classifier_results = classifier_results.Clone();
  backend_->SetContentAnnotationsCacheData(url, std::move(data));

  base::optional_ref<
      const AccessibilityAnnotatorBackend::ContentAnnotationsData>
      cached_data = backend_->GetContentAnnotationsCacheData(url);
  ASSERT_TRUE(cached_data.has_value());
  EXPECT_EQ(cached_data->page_title, page_title);
  ASSERT_TRUE(cached_data->tab_id.has_value());
  EXPECT_EQ(*cached_data->tab_id, 123);
  EXPECT_EQ(cached_data->annotations, annotations);
  EXPECT_EQ(cached_data->classifier_results, classifier_results);
  EXPECT_EQ(cached_data->visit_id, static_cast<history::VisitID>(123));
  EXPECT_EQ(cached_data->navigation_timestamp, now);
}

TEST_F(AccessibilityAnnotatorBackendTest, GetDebugUICacheDataEmpty) {
  base::Value result = backend_->GetDebugUICacheData();
  ASSERT_TRUE(result.is_list());
  EXPECT_TRUE(result.GetList().empty());
}

TEST_F(AccessibilityAnnotatorBackendTest, GetDebugUICacheDataWithEntries) {
  GURL url("https://example.com/path?query=1&other=2");
  std::string page_title = "Test Page Title";
  base::DictValue annotations;
  annotations.Set("1", "value1");
  annotations.Set("2", "value2");
  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "test category");

  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.annotations = annotations.Clone();
  data.classifier_results = classifier_results.Clone();
  backend_->SetContentAnnotationsCacheData(url, std::move(data));

  base::Value result = backend_->GetDebugUICacheData();
  ASSERT_TRUE(result.is_list());
  const base::ListValue& list = result.GetList();
  ASSERT_EQ(list.size(), 1u);

  const base::DictValue& entry = list[0].GetDict();
  EXPECT_THAT(entry,
              DictionaryHasValues(
                  base::DictValue()
                      .Set("url", url.spec())
                      .Set("title", page_title)
                      .Set("tab_id", 123)
                      .Set("annotations", annotations.Clone())
                      .Set("classifier_results", classifier_results.Clone())));
}

TEST_F(AccessibilityAnnotatorBackendTest, GetDebugUICacheDataWithProtoEntries) {
  GURL url("https://example.com/path?query=1&other=2");
  std::string page_title = "Test Page Title";
  optimization_guide::proto::ContentAnnotation content_annotation;
  content_annotation.set_description("Test annotation description");
  content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  auto* order = content_annotation.mutable_structured_data()->add_orders();
  order->set_id("order_123");

  base::DictValue classifier_results;
  classifier_results.Set("url_match_result", "test category");

  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.classifier_results = classifier_results.Clone();
  data.content_annotation = content_annotation;
  backend_->SetContentAnnotationsCacheData(url, std::move(data));

  base::Value result = backend_->GetDebugUICacheData();
  ASSERT_TRUE(result.is_list());
  const base::ListValue& list = result.GetList();
  ASSERT_EQ(list.size(), 1u);

  const base::DictValue& entry = list[0].GetDict();
  EXPECT_THAT(
      entry,
      DictionaryHasValues(
          base::DictValue()
              .Set("url", url.spec())
              .Set("title", page_title)
              .Set("classifier_results", classifier_results.Clone())
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

TEST_F(AccessibilityAnnotatorBackendTest,
       SetContentAnnotationsCacheDataWithOnlyProto) {
  GURL url("https://example.com/");
  std::string page_title = "Test Page Title";
  optimization_guide::proto::ContentAnnotation content_annotation;
  content_annotation.set_description("Only proto description");
  content_annotation.set_status(
      optimization_guide::proto::ContentAnnotation::CONFIRMED);
  content_annotation.mutable_structured_data()->add_orders()->set_id(
      "order_123");

  base::Time now = base::Time::Now();
  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData(page_title);
  data.navigation_timestamp = now;
  data.content_annotation = content_annotation;
  backend_->SetContentAnnotationsCacheData(url, std::move(data));

  base::optional_ref<
      const AccessibilityAnnotatorBackend::ContentAnnotationsData>
      cached_data = backend_->GetContentAnnotationsCacheData(url);
  ASSERT_TRUE(cached_data.has_value());
  EXPECT_EQ(cached_data->page_title, page_title);
  EXPECT_FALSE(cached_data->annotations.has_value());
  ASSERT_TRUE(cached_data->content_annotation.has_value());
  EXPECT_EQ(cached_data->content_annotation->description(),
            "Only proto description");
  EXPECT_EQ(cached_data->content_annotation->status(),
            optimization_guide::proto::ContentAnnotation::CONFIRMED);
  ASSERT_EQ(cached_data->content_annotation->structured_data().orders_size(),
            1);
  EXPECT_EQ(cached_data->content_annotation->structured_data().orders(0).id(),
            "order_123");
  EXPECT_EQ(cached_data->visit_id, static_cast<history::VisitID>(123));
  EXPECT_EQ(cached_data->navigation_timestamp, now);
}

TEST_F(AccessibilityAnnotatorBackendTest, RemoveContentAnnotationsCacheData) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");
  GURL url3("https://example.com/3");

  backend_->SetContentAnnotationsCacheData(
      url1, CreateContentAnnotationsData("Page 1"));
  backend_->SetContentAnnotationsCacheData(
      url2, CreateContentAnnotationsData("Page 2"));
  backend_->SetContentAnnotationsCacheData(
      url3, CreateContentAnnotationsData("Page 3"));

  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(url1).has_value());
  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(url2).has_value());
  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(url3).has_value());

  backend_->RemoveContentAnnotationsCacheData({url1, url2});
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(url1).has_value());
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(url2).has_value());
  EXPECT_TRUE(backend_->GetContentAnnotationsCacheData(url3).has_value());

  backend_->RemoveContentAnnotationsCacheData({url3});
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(url3).has_value());
}

TEST_F(AccessibilityAnnotatorBackendTest, ClearContentAnnotationsCache) {
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  backend_->SetContentAnnotationsCacheData(
      url1, CreateContentAnnotationsData("Page 1"));
  backend_->SetContentAnnotationsCacheData(
      url2, CreateContentAnnotationsData("Page 2"));

  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(url1).has_value());
  ASSERT_TRUE(backend_->GetContentAnnotationsCacheData(url2).has_value());

  backend_->ClearContentAnnotationsCache();
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(url1).has_value());
  EXPECT_FALSE(backend_->GetContentAnnotationsCacheData(url2).has_value());
}

TEST_F(AccessibilityAnnotatorBackendTest, ObserverNotified) {
  MockBackendObserver observer;
  backend_->AddObserver(&observer);

  GURL url("https://example.com/");
  AccessibilityAnnotatorBackend::ContentAnnotationsData data =
      CreateContentAnnotationsData("Test Page Title");
  data.content_annotation = optimization_guide::proto::ContentAnnotation();
  data.content_annotation->set_description("Test Description");

  EXPECT_CALL(
      observer,
      OnContentAnnotationsAdded(AllOf(
          // Match the plain struct member
          Field(&AccessibilityAnnotatorBackend::ContentAnnotationsData::
                    page_title,
                Eq("Test Page Title")),

          // Match the std::optional field
          Field(&AccessibilityAnnotatorBackend::ContentAnnotationsData::
                    content_annotation,
                Optional(Property(
                    &optimization_guide::proto::ContentAnnotation::description,
                    Eq("Test Description")))))));
  backend_->SetContentAnnotationsCacheData(url, std::move(data));
  testing::Mock::VerifyAndClearExpectations(&observer);

  backend_->RemoveObserver(&observer);
}

}  // namespace
}  // namespace accessibility_annotator
