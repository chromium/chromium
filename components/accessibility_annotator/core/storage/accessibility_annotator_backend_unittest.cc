// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/types/optional_ref.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {
namespace {

using ::testing::Eq;
using ::testing::Pointee;

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

  AccessibilityAnnotatorBackend::ContentAnnotationsData data;
  data.page_title = page_title;
  data.tab_id = 123;
  data.annotations = annotations.Clone();
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

  AccessibilityAnnotatorBackend::ContentAnnotationsData data;
  data.page_title = page_title;
  data.tab_id = 123;
  data.annotations = annotations.Clone();
  data.classifier_results = classifier_results.Clone();
  backend_->SetContentAnnotationsCacheData(url, std::move(data));

  base::Value result = backend_->GetDebugUICacheData();
  ASSERT_TRUE(result.is_list());
  const base::ListValue& list = result.GetList();
  ASSERT_EQ(list.size(), 1u);

  const base::DictValue& entry = list[0].GetDict();
  EXPECT_THAT(entry.FindString("url"), Pointee(Eq(url.spec())));
  EXPECT_THAT(entry.FindString("title"), Pointee(Eq(page_title)));
  EXPECT_THAT(entry.FindInt("tab_id"), 123);

  const base::DictValue* annotations_dict = entry.FindDict("annotations");
  ASSERT_TRUE(annotations_dict);
  EXPECT_THAT(annotations_dict->FindString("1"), Pointee(Eq("value1")));
  EXPECT_THAT(annotations_dict->FindString("2"), Pointee(Eq("value2")));

  const base::DictValue* classifier_results_dict =
      entry.FindDict("classifier_results");
  ASSERT_TRUE(classifier_results_dict);
  EXPECT_THAT(classifier_results_dict->FindString("url_match_result"),
              Pointee(Eq("test category")));
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

  AccessibilityAnnotatorBackend::ContentAnnotationsData data;
  data.page_title = page_title;
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
}

}  // namespace
}  // namespace accessibility_annotator
