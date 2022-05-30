// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/test/ranking_test_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/chrome_ash_test_base.h"

namespace app_list {
namespace test {

using ::test::TestAppListControllerDelegate;

// SearchControllerImplTest
// --------------------------------------------------------

class SearchControllerImplTest : public ChromeAshTestBase {
 public:
  SearchControllerImplTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kProductivityLauncher);
  }
  SearchControllerImplTest(const SearchControllerImplTest&) = delete;
  SearchControllerImplTest& operator=(const SearchControllerImplTest&) = delete;
  ~SearchControllerImplTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    search_controller_ = std::make_unique<SearchControllerImpl>(
        /*model_updater=*/nullptr, &list_controller_, /*profile=*/nullptr,
        /*notifier=*/nullptr);
  }
  SearchController& search_controller() { return *search_controller_; }
  TestAppListControllerDelegate& list_controller() { return list_controller_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestAppListControllerDelegate list_controller_;
  std::unique_ptr<SearchControllerImpl> search_controller_;
};

// Tests -----------------------------------------------------------------------

TEST_F(SearchControllerImplTest,
       ShouldConditionallyDismissViewWhenOpeningResult) {
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

  auto result = std::make_unique<TestResult>();
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
