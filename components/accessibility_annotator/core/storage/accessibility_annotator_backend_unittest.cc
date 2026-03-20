// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {
namespace {

class AccessibilityAnnotatorBackendTest : public testing::Test {
 public:
  AccessibilityAnnotatorBackendTest() = default;
  ~AccessibilityAnnotatorBackendTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_ = std::make_unique<AccessibilityAnnotatorBackend>(
        version_info::Channel::UNKNOWN, /*history_service=*/nullptr,
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        temp_dir_.GetPath().AppendASCII("TestDB"));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AccessibilityAnnotatorBackend> backend_;
};

TEST_F(AccessibilityAnnotatorBackendTest, GetContentAnnotationsCacheData) {
  GURL url("https://example.com/");
  std::string page_title = "Test Page Title";
  std::string annotations = R"({"1": "value1"})";

  // Cache should be empty initially.
  ASSERT_FALSE(backend_->GetContentAnnotationsCacheData(url).has_value());

  backend_->SetContentAnnotationsCacheData(url, page_title, annotations);

  std::optional<AccessibilityAnnotatorBackend::ContentAnnotationsData>
      cached_data = backend_->GetContentAnnotationsCacheData(url);
  ASSERT_TRUE(cached_data.has_value());
  EXPECT_EQ(cached_data->page_title, page_title);
  EXPECT_EQ(cached_data->annotations, annotations);
}

TEST_F(AccessibilityAnnotatorBackendTest, GetDebugUICacheDataEmpty) {
  base::Value result = backend_->GetDebugUICacheData();
  ASSERT_TRUE(result.is_list());
  EXPECT_TRUE(result.GetList().empty());
}

TEST_F(AccessibilityAnnotatorBackendTest, GetDebugUICacheDataWithEntries) {
  GURL url("https://example.com/path?query=1&other=2");
  std::string page_title = "Test Page Title";
  std::string annotations = R"({"1": "value1", "2": "value2"})";
  backend_->SetContentAnnotationsCacheData(url, page_title, annotations);

  base::Value result = backend_->GetDebugUICacheData();
  ASSERT_TRUE(result.is_list());
  const base::ListValue& list = result.GetList();
  ASSERT_EQ(list.size(), 1u);

  const base::DictValue& entry = list[0].GetDict();
  EXPECT_EQ(*entry.FindString("url"), url.spec());
  EXPECT_EQ(*entry.FindString("title"), page_title);

  const base::DictValue* annotations_dict = entry.FindDict("annotations");
  ASSERT_TRUE(annotations_dict);
  EXPECT_EQ(*annotations_dict->FindString("1"), "value1");
  EXPECT_EQ(*annotations_dict->FindString("2"), "value2");
}

}  // namespace
}  // namespace accessibility_annotator
