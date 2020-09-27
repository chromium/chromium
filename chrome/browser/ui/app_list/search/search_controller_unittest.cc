// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/test/shell_test_api.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/chrome_ash_test_base.h"

namespace app_list {
namespace test {

using ::test::TestAppListControllerDelegate;

// TestSearchResult ------------------------------------------------------------

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult() = default;
  TestSearchResult(const TestSearchResult&) = delete;
  TestSearchResult& operator=(const TestSearchResult&) = delete;
  ~TestSearchResult() override = default;

 private:
  void Open(int event_flags) override { NOTIMPLEMENTED(); }
};

// SearchControllerTest --------------------------------------------------------

class SearchControllerTest : public ChromeAshTestBase {
 public:
  SearchControllerTest() = default;
  SearchControllerTest(const SearchControllerTest&) = delete;
  SearchControllerTest& operator=(const SearchControllerTest&) = delete;
  ~SearchControllerTest() override = default;

  SearchController& search_controller() { return search_controller_; }
  TestAppListControllerDelegate& list_controller() { return list_controller_; }

 private:
  TestAppListControllerDelegate list_controller_;
  SearchController search_controller_{/*model_updater=*/nullptr,
                                      &list_controller_, /*profile=*/nullptr,
                                      /*notifier=*/nullptr};
};

// Tests -----------------------------------------------------------------------

TEST_F(SearchControllerTest, ShouldConditionallyDismissViewWhenOpeningResult) {
  struct TestCase {
    bool is_tablet_mode = false;
    bool request_to_dismiss_view = false;
    bool expect_to_dismiss_view = false;
  };

  std::vector<TestCase> test_cases;

  test_cases.push_back({/*is_tablet_mode=*/false,
                        /*request_to_dismiss_view=*/false,
                        /*expect_to_dismiss_view=*/false});

  test_cases.push_back({/*is_tablet_mode=*/true,
                        /*request_to_dismiss_view=*/false,
                        /*expect_to_dismiss_view=*/false});

  test_cases.push_back({/*is_tablet_mode=*/true,
                        /*request_to_dismiss_view=*/true,
                        /*expect_to_dismiss_view=*/false});

  test_cases.push_back({/*is_tablet_mode=*/false,
                        /*request_to_dismiss_view=*/true,
                        /*expect_to_dismiss_view=*/true});

  auto result = std::make_unique<TestSearchResult>();
  for (auto& test_case : test_cases) {
    ash::ShellTestApi().SetTabletModeEnabledForTest(test_case.is_tablet_mode);

    result->set_dismiss_view_on_open(test_case.request_to_dismiss_view);
    search_controller().OpenResult(result.get(), /*event_flags=*/0);

    EXPECT_EQ(test_case.expect_to_dismiss_view,
              list_controller().did_dismiss_view());

    list_controller().Reset();
  }
}

}  // namespace test
}  // namespace app_list
