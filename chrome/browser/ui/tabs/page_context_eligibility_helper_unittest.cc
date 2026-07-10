// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace {

class MockEligibilityAPI {
 public:
  MOCK_METHOD(optimization_guide::PageEligibilityResult,
              CheckPageEligibility,
              (const std::vector<optimization_guide::FrameUrl>&));
};

MockEligibilityAPI* g_mock_api = nullptr;
bool g_mock_is_eligible_with_account = true;

bool MockIsPageContextEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<optimization_guide::FrameMetadata>& metadata) {
  return true;
}

bool MockIsPageContextEligibleWithAccount(
    const std::string& host,
    const std::string& path,
    const std::string& account,
    const std::vector<optimization_guide::FrameMetadata>& metadata) {
  return g_mock_is_eligible_with_account;
}

bool MockShouldReextractPageContext(
    const std::string& host,
    const std::string& path,
    const std::vector<std::string>& updated_meta_tags) {
  return false;
}

optimization_guide::PageEligibilityResult MockCheckPageEligibility(
    const std::vector<optimization_guide::FrameUrl>& frames) {
  if (g_mock_api) {
    return g_mock_api->CheckPageEligibility(frames);
  }
  return optimization_guide::PageEligibilityResult{
      .status = optimization_guide::PageEligibility::kEligible,
      .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
}

optimization_guide::PageContextEligibilityAPI test_api = {
    .IsPageContextEligible = &MockIsPageContextEligible,
    .IsPageContextEligibleWithAccount = &MockIsPageContextEligibleWithAccount,
    .ShouldReextractPageContext = &MockShouldReextractPageContext,
    .GetMetaTagNamesAffectingEligibility = nullptr,
    .CheckPageEligibility = &MockCheckPageEligibility,
};

}  // namespace

namespace tabs {

class PageContextEligibilityHelperTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PageContextEligibilityHelperTest() = default;
  ~PageContextEligibilityHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_api_ = std::make_unique<testing::StrictMock<MockEligibilityAPI>>();
    g_mock_api = mock_api_.get();
    g_mock_is_eligible_with_account = true;
    test_eligibility_holder_ =
        std::make_unique<optimization_guide::PageContextEligibility>(&test_api);
    optimization_guide::PageContextEligibility::SetForTesting(
        test_eligibility_holder_.get());

    mock_tab_ = std::make_unique<MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(*mock_tab_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(*mock_tab_, IsActivated()).WillByDefault(Return(true));
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));
  }

  void TearDown() override {
    helper_.reset();
    mock_tab_.reset();
    optimization_guide::PageContextEligibility::SetForTesting(nullptr);
    g_mock_api = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateHelper() {
    helper_ = std::make_unique<PageContextEligibilityHelper>(*mock_tab_);
    WaitForEligibility(helper_.get());
  }

  void WaitForEligibility(PageContextEligibilityHelper* helper) {
    base::RunLoop run_loop;
    base::CallbackListSubscription sub =
        helper->RegisterEligibilityChangeCallback(base::BindRepeating(
            [](base::RunLoop* run_loop,
               optimization_guide::PageContextEligibilityStatus val) {
              if (val !=
                  optimization_guide::PageContextEligibilityStatus::kUnknown) {
                run_loop->Quit();
              }
            },
            &run_loop));
    run_loop.Run();
  }

  void TriggerOnTabActivated(PageContextEligibilityHelper* helper,
                             tabs::TabInterface* tab) {
    helper->OnTabActivated(tab);
  }
  void TriggerOnTabDeactivated(PageContextEligibilityHelper* helper,
                               tabs::TabInterface* tab) {
    helper->OnTabDeactivated(tab);
  }
  void TriggerOnDiscardContents(PageContextEligibilityHelper* helper,
                                tabs::TabInterface* tab,
                                content::WebContents* old_contents,
                                content::WebContents* new_contents) {
    helper->OnDiscardContents(tab, old_contents, new_contents);
  }
  void TriggerOnWillDetach(PageContextEligibilityHelper* helper,
                           tabs::TabInterface* tab,
                           tabs::TabInterface::DetachReason reason) {
    helper->OnWillDetach(tab, reason);
  }

  void SetMockEligibility(bool eligible) {
    g_mock_is_eligible_with_account = eligible;
    static int count = 0;
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(
            GURL("https://example.com/" + base::NumberToString(++count)));
    task_environment()->RunUntilIdle();
  }

 protected:
  std::unique_ptr<MockEligibilityAPI> mock_api_;
  std::unique_ptr<optimization_guide::PageContextEligibility>
      test_eligibility_holder_;

  std::unique_ptr<MockTabInterface> mock_tab_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<PageContextEligibilityHelper> helper_;
};

TEST_F(PageContextEligibilityHelperTest, BasicEligibility) {
  EXPECT_CALL(*mock_api_, CheckPageEligibility(testing::_))
      .WillRepeatedly(Return(optimization_guide::PageEligibilityResult{
          .status = optimization_guide::PageEligibility::kEligible,
          .meta_tag_names_affecting_eligibility = {.data = nullptr,
                                                   .size = 0}}));

  CreateHelper();

  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);

  SetMockEligibility(true);
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);

  SetMockEligibility(false);
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kNotEligible);
}

TEST_F(PageContextEligibilityHelperTest, CallbacksNotified) {
  EXPECT_CALL(*mock_api_, CheckPageEligibility(testing::_))
      .WillRepeatedly(Return(optimization_guide::PageEligibilityResult{
          .status = optimization_guide::PageEligibility::kEligible,
          .meta_tag_names_affecting_eligibility = {.data = nullptr,
                                                   .size = 0}}));

  CreateHelper();

  optimization_guide::PageContextEligibilityStatus callback_val =
      optimization_guide::PageContextEligibilityStatus::kEligible;
  int callback_count = 0;
  base::CallbackListSubscription sub =
      helper_->RegisterEligibilityChangeCallback(base::BindRepeating(
          [](optimization_guide::PageContextEligibilityStatus* out_val,
             int* out_count,
             optimization_guide::PageContextEligibilityStatus val) {
            *out_val = val;
            (*out_count)++;
          },
          &callback_val, &callback_count));

  SetMockEligibility(false);
  EXPECT_EQ(callback_val,
            optimization_guide::PageContextEligibilityStatus::kNotEligible);
  EXPECT_EQ(2, callback_count);

  SetMockEligibility(true);
  EXPECT_EQ(callback_val,
            optimization_guide::PageContextEligibilityStatus::kEligible);
  EXPECT_EQ(4, callback_count);
}

TEST_F(PageContextEligibilityHelperTest, TabActivationDeactivation) {
  EXPECT_CALL(*mock_api_, CheckPageEligibility(testing::_))
      .WillRepeatedly(Return(optimization_guide::PageEligibilityResult{
          .status = optimization_guide::PageEligibility::kEligible,
          .meta_tag_names_affecting_eligibility = {.data = nullptr,
                                                   .size = 0}}));

  CreateHelper();

  // Initially active, helper has valid eligibility
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);

  // Deactivate the tab: observer is destroyed, returns kUnknown
  TriggerOnTabDeactivated(helper_.get(), &*mock_tab_);
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kUnknown);

  // Activate the tab: observer is recreated, returns kEligible
  TriggerOnTabActivated(helper_.get(), &*mock_tab_);
  WaitForEligibility(helper_.get());
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);
}

TEST_F(PageContextEligibilityHelperTest, NavigationUpdatesEligibility) {
  EXPECT_CALL(*mock_api_, CheckPageEligibility(testing::_))
      .WillRepeatedly(Return(optimization_guide::PageEligibilityResult{
          .status = optimization_guide::PageEligibility::kEligible,
          .meta_tag_names_affecting_eligibility = {.data = nullptr,
                                                   .size = 0}}));

  CreateHelper();
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);

  std::vector<optimization_guide::PageContextEligibilityStatus> updates;
  updates.push_back(helper_->IsPageContextEligible());

  base::CallbackListSubscription sub =
      helper_->RegisterEligibilityChangeCallback(base::BindRepeating(
          [](std::vector<optimization_guide::PageContextEligibilityStatus>*
                 out_updates,
             optimization_guide::PageContextEligibilityStatus val) {
            out_updates->push_back(val);
          },
          &updates));

  SetMockEligibility(false);

  std::vector<optimization_guide::PageContextEligibilityStatus>
      expected_updates = {
          optimization_guide::PageContextEligibilityStatus::kEligible,
          optimization_guide::PageContextEligibilityStatus::kUnknown,
          optimization_guide::PageContextEligibilityStatus::kNotEligible,
      };
  EXPECT_EQ(updates, expected_updates);
}

TEST_F(PageContextEligibilityHelperTest,
       DiscardContentsRecreatesObserverWithNewContents) {
  EXPECT_CALL(*mock_api_, CheckPageEligibility(testing::_))
      .WillRepeatedly(Return(optimization_guide::PageEligibilityResult{
          .status = optimization_guide::PageEligibility::kEligible,
          .meta_tag_names_affecting_eligibility = {.data = nullptr,
                                                   .size = 0}}));
  SetMockEligibility(true);
  CreateHelper();
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);

  std::vector<optimization_guide::PageContextEligibilityStatus> updates;
  base::CallbackListSubscription sub =
      helper_->RegisterEligibilityChangeCallback(base::BindRepeating(
          [](std::vector<optimization_guide::PageContextEligibilityStatus>*
                 out_updates,
             optimization_guide::PageContextEligibilityStatus val) {
            out_updates->push_back(val);
          },
          &updates));

  std::unique_ptr<content::WebContents> new_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(new_contents.get())
      ->NavigateAndCommit(GURL("https://www.example.com"));

  TriggerOnDiscardContents(helper_.get(), &*mock_tab_, web_contents(),
                           new_contents.get());
  WaitForEligibility(helper_.get());

  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);
  std::vector<optimization_guide::PageContextEligibilityStatus>
      expected_updates = {
          optimization_guide::PageContextEligibilityStatus::kUnknown,
          optimization_guide::PageContextEligibilityStatus::kEligible,
      };
  EXPECT_EQ(updates, expected_updates);
}

TEST_F(PageContextEligibilityHelperTest, WillDetachDeleteResetsToUnknown) {
  EXPECT_CALL(*mock_api_, CheckPageEligibility(testing::_))
      .WillRepeatedly(Return(optimization_guide::PageEligibilityResult{
          .status = optimization_guide::PageEligibility::kEligible,
          .meta_tag_names_affecting_eligibility = {.data = nullptr,
                                                   .size = 0}}));
  SetMockEligibility(true);
  CreateHelper();
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kEligible);

  std::vector<optimization_guide::PageContextEligibilityStatus> updates;
  base::CallbackListSubscription sub =
      helper_->RegisterEligibilityChangeCallback(base::BindRepeating(
          [](std::vector<optimization_guide::PageContextEligibilityStatus>*
                 out_updates,
             optimization_guide::PageContextEligibilityStatus val) {
            out_updates->push_back(val);
          },
          &updates));

  TriggerOnWillDetach(helper_.get(), &*mock_tab_,
                      tabs::TabInterface::DetachReason::kDelete);
  EXPECT_EQ(helper_->IsPageContextEligible(),
            optimization_guide::PageContextEligibilityStatus::kUnknown);
  EXPECT_EQ(
      updates,
      std::vector<optimization_guide::PageContextEligibilityStatus>{
          optimization_guide::PageContextEligibilityStatus::kUnknown});
}

}  // namespace tabs
