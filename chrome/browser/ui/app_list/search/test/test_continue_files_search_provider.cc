// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/test/test_continue_files_search_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {

namespace {

// Fake continue section search results.
class TestContinueSectionSearchResult : public ChromeSearchResult {
 public:
  explicit TestContinueSectionSearchResult(const std::string& id) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    SetDisplayType(DisplayType::kContinue);
    SetCategory(Category::kFiles);
    SetResultType(ResultType::kZeroStateFile);
    SetMetricsType(ash::ZERO_STATE_FILE);
  }

  TestContinueSectionSearchResult(const TestContinueSectionSearchResult&) =
      delete;
  TestContinueSectionSearchResult& operator=(
      const TestContinueSectionSearchResult&) = delete;

  ~TestContinueSectionSearchResult() override = default;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

}  // namespace

TestContinueFilesSearchProvider::TestContinueFilesSearchProvider() = default;

TestContinueFilesSearchProvider::~TestContinueFilesSearchProvider() = default;

void TestContinueFilesSearchProvider::StartZeroState() {
  DCHECK(app_list_features::IsCategoricalSearchEnabled());

  auto create_result = [](int index) -> std::unique_ptr<ChromeSearchResult> {
    const std::string id = base::StringPrintf("continue_task_%d", index);
    return std::make_unique<TestContinueSectionSearchResult>(id);
  };

  std::vector<std::unique_ptr<ChromeSearchResult>> results;
  for (size_t i = 0; i < count_; ++i)
    results.push_back(create_result(i));

  SwapResults(&results);
}

ash::AppListSearchResultType TestContinueFilesSearchProvider::ResultType()
    const {
  return ResultType::kUnknown;
}

bool TestContinueFilesSearchProvider::ShouldBlockZeroState() const {
  return true;
}

}  // namespace app_list
