// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/chip_selector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/product_messaging/product_messaging_controller.h"
#include "components/user_education/product_messaging/product_messaging_policy_impl.h"
#include "components/user_education/product_messaging/product_messaging_types.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"

const char kActiveChipsHistogram[] =
    "PageActionController.ActiveSuggestionChips";

namespace page_actions {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

class DefaultChipSelectorTest : public testing::Test {
 public:
  void SetUp() override {
    selector = std::make_unique<internal::DefaultChipSelector>(
        base::BindRepeating(&DefaultChipSelectorTest::ShowChipCallback,
                            base::Unretained(this)),
        base::BindRepeating(&DefaultChipSelectorTest::HideChipCallback,
                            base::Unretained(this)),
        base::BindRepeating(
            &DefaultChipSelectorTest::ShowAnchoredMessageCallback,
            base::Unretained(this)),
        base::BindRepeating(
            &DefaultChipSelectorTest::HideAnchoredMessageCallback,
            base::Unretained(this)));
  }

 private:
  void ShowChipCallback(actions::ActionId page_action_id,
                        const SuggestionChipConfig& config) {
    calls.emplace_back("show_chip", page_action_id);
  }

  void HideChipCallback(actions::ActionId page_action_id) {
    calls.emplace_back("hide_chip", page_action_id);
  }

  void ShowAnchoredMessageCallback(actions::ActionId page_action_id,
                                   const AnchoredMessageConfig& config) {
    calls.emplace_back("show_anchored_message", page_action_id);
  }

  void HideAnchoredMessageCallback(actions::ActionId page_action_id) {
    calls.emplace_back("hide_anchored_message", page_action_id);
  }

 public:
  std::unique_ptr<internal::DefaultChipSelector> selector;
  std::vector<std::pair<std::string, actions::ActionId>> calls;
};

TEST_F(DefaultChipSelectorTest, ShowSingleChip) {
  base::HistogramTester histogram_tester;
  selector->RequestChipShow(0, SuggestionChipConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
  EXPECT_EQ(histogram_tester.GetBucketCount(kActiveChipsHistogram, 1), 1);
}

TEST_F(DefaultChipSelectorTest, ShowChipTwice) {
  base::HistogramTester histogram_tester;
  selector->RequestChipShow(0, SuggestionChipConfig{});
  selector->RequestChipShow(0, SuggestionChipConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("show_chip", 0)));
  EXPECT_EQ(histogram_tester.GetBucketCount(kActiveChipsHistogram, 1), 1);
}

TEST_F(DefaultChipSelectorTest, ShowTwoChips) {
  base::HistogramTester histogram_tester;
  selector->RequestChipShow(0, SuggestionChipConfig{});
  selector->RequestChipShow(1, SuggestionChipConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("show_chip", 1)));
  EXPECT_EQ(histogram_tester.GetBucketCount(kActiveChipsHistogram, 1), 1);
  EXPECT_EQ(histogram_tester.GetBucketCount(kActiveChipsHistogram, 2), 1);
}

TEST_F(DefaultChipSelectorTest, HideUnshownChip) {
  selector->RequestChipHide(0);
  EXPECT_THAT(calls, ElementsAre(Pair("hide_chip", 0)));
}

TEST_F(DefaultChipSelectorTest, HideShownChip) {
  selector->RequestChipShow(0, SuggestionChipConfig{});
  selector->RequestChipHide(0);
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("hide_chip", 0)));
}

TEST_F(DefaultChipSelectorTest, HistogramShowHideShow) {
  base::HistogramTester histogram_tester;
  selector->RequestChipShow(0, SuggestionChipConfig{});
  selector->RequestChipHide(0);
  selector->RequestChipShow(0, SuggestionChipConfig{});
  EXPECT_EQ(histogram_tester.GetBucketCount(kActiveChipsHistogram, 1), 2);
  EXPECT_EQ(histogram_tester.GetBucketCount(kActiveChipsHistogram, 2), 0);
}

TEST_F(DefaultChipSelectorTest, AnchoredMessageHidesChip) {
  selector->RequestChipShow(0, SuggestionChipConfig{});
  selector->RequestAnchoredMessageShow(0, AnchoredMessageConfig{});
  EXPECT_THAT(
      calls, ElementsAre(Pair("show_chip", 0), Pair("show_anchored_message", 0),
                         Pair("hide_chip", 0)));
}

TEST_F(DefaultChipSelectorTest, ChipHidesAnchoredMessage) {
  selector->RequestAnchoredMessageShow(0, AnchoredMessageConfig{});
  selector->RequestChipShow(0, SuggestionChipConfig{});
  EXPECT_THAT(
      calls, ElementsAre(Pair("show_anchored_message", 0), Pair("show_chip", 0),
                         Pair("hide_anchored_message", 0)));
}

TEST_F(DefaultChipSelectorTest, OnlyFirstAnchoredMessageShows) {
  selector->RequestAnchoredMessageShow(0, AnchoredMessageConfig{});
  selector->RequestAnchoredMessageShow(1, AnchoredMessageConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(DefaultChipSelectorTest, AnchoredMessageQueue) {
  selector->RequestAnchoredMessageShow(0, AnchoredMessageConfig{});
  selector->RequestAnchoredMessageShow(1, AnchoredMessageConfig{});
  selector->RequestAnchoredMessageShow(2, AnchoredMessageConfig{});
  selector->RequestAnchoredMessageHide(1);
  selector->RequestAnchoredMessageHide(0);
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("hide_anchored_message", 0),
                                 Pair("show_anchored_message", 2)));
}

TEST_F(DefaultChipSelectorTest, HideUnshownAnchoredMessage) {
  selector->RequestAnchoredMessageHide(0);
  EXPECT_THAT(calls, IsEmpty());
}

TEST_F(DefaultChipSelectorTest,
       UserInteractionAnchoredMessageDowngradesCurrent) {
  selector->RequestAnchoredMessageShow(0, AnchoredMessageConfig{});

  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  EXPECT_THAT(
      calls, ElementsAre(Pair("show_anchored_message", 0),
                         Pair("hide_anchored_message", 0), Pair("show_chip", 0),
                         Pair("show_anchored_message", 1)));
}

TEST_F(DefaultChipSelectorTest, UserInteractionAnchoredMessageReplacesChip) {
  selector->RequestChipShow(0, {});
  selector->RequestAnchoredMessageShow(
      0, {.priority = PageActionPriorityCategory::kUserInteraction});
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("hide_chip", 0),
                                 Pair("show_anchored_message", 0)));
}

TEST_F(DefaultChipSelectorTest,
       UserInteractionAnchoredMessageReplacesFrontOfQueue) {
  selector->RequestAnchoredMessageShow(0, AnchoredMessageConfig{});
  selector->RequestAnchoredMessageShow(1, AnchoredMessageConfig{});
  calls.clear();

  selector->RequestAnchoredMessageShow(
      2, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  EXPECT_THAT(
      calls, ElementsAre(Pair("hide_anchored_message", 0), Pair("show_chip", 0),
                         Pair("show_anchored_message", 2)));

  // Verify queue state by hiding the current message and checking if the next
  // one shows.
  calls.clear();
  selector->RequestAnchoredMessageHide(2);
  EXPECT_THAT(calls, ElementsAre(Pair("hide_anchored_message", 2),
                                 Pair("show_anchored_message", 1)));
}

TEST_F(DefaultChipSelectorTest, OnTabActiveChangedNoOp) {
  selector->OnTabActiveChanged(false);
  selector->RequestAnchoredMessageShow(0, AnchoredMessageConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

class PriorityChipSelectorTest : public testing::Test {
 public:
  void SetUp() override {
    selector = std::make_unique<internal::PriorityChipSelector>(
        base::BindRepeating(&PriorityChipSelectorTest::ShowChipCallback,
                            base::Unretained(this)),
        base::BindRepeating(&PriorityChipSelectorTest::HideChipCallback,
                            base::Unretained(this)),
        base::BindRepeating(
            &PriorityChipSelectorTest::ShowAnchoredMessageCallback,
            base::Unretained(this)),
        base::BindRepeating(
            &PriorityChipSelectorTest::HideAnchoredMessageCallback,
            base::Unretained(this)));
  }

 protected:
  void ShowChipCallback(actions::ActionId page_action_id,
                        const SuggestionChipConfig& config) {
    calls.emplace_back("show_chip", page_action_id);
  }

  void HideChipCallback(actions::ActionId page_action_id) {
    calls.emplace_back("hide_chip", page_action_id);
  }

  void ShowAnchoredMessageCallback(actions::ActionId page_action_id,
                                   const AnchoredMessageConfig& config) {
    calls.emplace_back("show_anchored_message", page_action_id);
  }

  void HideAnchoredMessageCallback(actions::ActionId page_action_id) {
    calls.emplace_back("hide_anchored_message", page_action_id);
  }

  std::unique_ptr<internal::PriorityChipSelector> selector;
  std::vector<std::pair<std::string, actions::ActionId>> calls;
};

TEST_F(PriorityChipSelectorTest, ShowSingleChip) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorTest, HigherPriorityPreemptsLowerPriority) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});
  selector->RequestChipShow(
      1, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("hide_chip", 0),
                                 Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorTest, LowerPriorityIgnored) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestChipShow(
      1, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorTest, SamePriorityIgnored) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});
  selector->RequestChipShow(
      1, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorTest, PrivacySecurityAllowsMultipleChips) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestChipShow(
      1, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorTest, AnchoredMessagePreemptsLowerPriorityChip) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("hide_chip", 0),
                                 Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorTest, PrivacySecurityAllowsChipAndMessage) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestChipShow(
      1, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorTest,
       ChipDoesntPreemptHigherPriorityAnchoredMessage) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestChipShow(
      1, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});

  // ContextualCue is lower than PrivacySecurity, so it should be ignored.
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(PriorityChipSelectorTest, HideClearsPriority) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestChipHide(0);

  // Now lower priority should be allowed.
  selector->RequestChipShow(
      1, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("hide_chip", 0),
                                 Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorTest, PrivacySecurityChipThenMessage) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0),
                                 Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorTest, PrivacySecurityMultipleChipsHideOne) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestChipShow(
      1, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  selector->RequestChipHide(0);

  // Priority should still be PrivacySecurity because of chip 1.
  selector->RequestChipShow(
      2, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("show_chip", 1),
                                 Pair("hide_chip", 0)));
}

TEST_F(PriorityChipSelectorTest, SamePriorityChipThenMessage) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});

  // Message should be ignored as a chip of the same priority is active.
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorTest,
       PrivacySecuritySecondAnchoredMessageDowngraded) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorTest, UserInteractionDowngradesAnchoredMessage) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});

  EXPECT_THAT(
      calls, ElementsAre(Pair("show_anchored_message", 0),
                         Pair("hide_anchored_message", 0), Pair("show_chip", 0),
                         Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorTest, UserInteractionPreemptsLowerPriority) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});

  // Lower priority (ContextualCue) should be hidden, not downgraded.
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0), Pair("hide_chip", 0),
                                 Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorTest, UserInteractionAlongsidePrivacySecurityChips) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});

  // PrivacySecurity chip should remain showing alongside UserInteraction
  // message.
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0),
                                 Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorTest,
       PrivacySecurityChipShownWhileUserInteractionActive) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  selector->RequestChipShow(
      1, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorTest,
       PrivacySecurityAnchoredMessageDowngradedWhileUserInteractionActive) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorTest,
       PrivacySecurityShowsAfterUserInteractionDismissed) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestAnchoredMessageHide(0);
  selector->RequestAnchoredMessageShow(
      2, {.priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(
      calls, ElementsAre(Pair("show_anchored_message", 0), Pair("show_chip", 1),
                         Pair("hide_anchored_message", 0),
                         Pair("show_anchored_message", 2)));
}

TEST_F(PriorityChipSelectorTest, AnchoredMessageDowngradedOnTabDeactivation) {
  base::test::ScopedFeatureList feature_list(
      features::kPageActionAnchoredMessageActiveTabOnly);

  selector->OnTabActiveChanged(true);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));

  selector->OnTabActiveChanged(false);
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("hide_anchored_message", 0),
                                 Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorTest,
       AnchoredMessageNotDowngradedOnTabDeactivationWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPageActionAnchoredMessageActiveTabOnly);

  selector->OnTabActiveChanged(true);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));

  selector->OnTabActiveChanged(false);
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(PriorityChipSelectorTest,
       AnchoredMessageUserInteractionNotDowngradedOnTabDeactivation) {
  base::test::ScopedFeatureList feature_list(
      features::kPageActionAnchoredMessageActiveTabOnly);

  selector->OnTabActiveChanged(true);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));

  selector->OnTabActiveChanged(false);
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(PriorityChipSelectorTest,
       AnchoredMessageDowngradedWhenRequestedOnInactiveTab) {
  base::test::ScopedFeatureList feature_list(
      features::kPageActionAnchoredMessageActiveTabOnly);

  selector->OnTabActiveChanged(false);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorTest,
       AnchoredMessageUserInteractionNotDowngradedWhenRequestedOnInactiveTab) {
  base::test::ScopedFeatureList feature_list(
      features::kPageActionAnchoredMessageActiveTabOnly);

  selector->OnTabActiveChanged(false);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(
    PriorityChipSelectorTest,
    AnchoredMessageNotDowngradedWhenRequestedOnInactiveTabWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPageActionAnchoredMessageActiveTabOnly);

  selector->OnTabActiveChanged(false);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(
    kTestBlockingNotice,
    user_education::ProductMessageType::kLegalOrComplianceNotice);

class PriorityChipSelectorPmcTest : public testing::Test {
 public:
  PriorityChipSelectorPmcTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kPageActionsPrioritySelector,
         features::kPageActionsPrioritySelectorProductMessagingController,
         features::kPageActionAnchoredMessageActiveTabOnly},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    pmc_.Init(session_provider_, storage_service_,
              user_education::ProductMessagingPolicyImpl::CreateDefault());
    selector = std::make_unique<internal::PriorityChipSelector>(
        base::BindRepeating(&PriorityChipSelectorPmcTest::ShowChipCallback,
                            base::Unretained(this)),
        base::BindRepeating(&PriorityChipSelectorPmcTest::HideChipCallback,
                            base::Unretained(this)),
        base::BindRepeating(
            &PriorityChipSelectorPmcTest::ShowAnchoredMessageCallback,
            base::Unretained(this)),
        base::BindRepeating(
            &PriorityChipSelectorPmcTest::HideAnchoredMessageCallback,
            base::Unretained(this)),
        &pmc_);
  }

  void ShowChipCallback(actions::ActionId page_action_id,
                        const SuggestionChipConfig& config) {
    calls.emplace_back("show_chip", page_action_id);
  }

  void HideChipCallback(actions::ActionId page_action_id) {
    calls.emplace_back("hide_chip", page_action_id);
  }

  void ShowAnchoredMessageCallback(actions::ActionId page_action_id,
                                   const AnchoredMessageConfig& config) {
    calls.emplace_back("show_anchored_message", page_action_id);
  }

  void HideAnchoredMessageCallback(actions::ActionId page_action_id) {
    calls.emplace_back("hide_anchored_message", page_action_id);
  }

  user_education::ProductMessagingHandle AcquireBlockingNotice() {
    user_education::ProductMessagingHandle blocking_handle;
    base::RunLoop run_loop;
    pmc().QueueMessage(
        kTestBlockingNotice,
        base::BindOnce(
            [](user_education::ProductMessagingHandle* handle_dest,
               base::RepeatingClosure quit_closure,
               user_education::ProductMessagingHandle h) {
              *handle_dest = std::move(h);
              (*handle_dest)->SetShown();
              quit_closure.Run();
            },
            &blocking_handle, run_loop.QuitClosure()));
    run_loop.Run();
    return blocking_handle;
  }

  user_education::ProductMessagingController& pmc() { return pmc_; }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  user_education::test::TestUserEducationSessionProvider session_provider_{
      false};
  user_education::test::TestUserEducationStorageService storage_service_;
  user_education::ProductMessagingController pmc_;
  std::unique_ptr<internal::PriorityChipSelector> selector;
  std::vector<std::pair<std::string, actions::ActionId>> calls;
};

TEST_F(PriorityChipSelectorPmcTest, ShowAnchoredMessage_WaitsForPmcPermission) {
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  EXPECT_THAT(calls, IsEmpty());
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kWaiting);

  ASSERT_TRUE(base::test::RunUntil([&]() { return !calls.empty(); }));
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kShowing);

  selector->RequestAnchoredMessageHide(0);
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("hide_anchored_message", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);
}

TEST_F(PriorityChipSelectorPmcTest,
       ShowAnchoredMessage_TimesOutAfter10sAndDowngradesToChip) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();
  ASSERT_TRUE(blocking_handle);

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  EXPECT_THAT(calls, IsEmpty());

  task_environment().FastForwardBy(base::Seconds(9));
  EXPECT_THAT(calls, IsEmpty());

  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);
}

TEST_F(PriorityChipSelectorPmcTest,
       UserInteractionAnchoredMessage_BypassesPmc) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();
  ASSERT_TRUE(blocking_handle);

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);
}

TEST_F(PriorityChipSelectorPmcTest, UserInteraction_CancelsPendingPmcMessage) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  EXPECT_THAT(calls, IsEmpty());

  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kUserInteraction});
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 1)));

  task_environment().FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorPmcTest,
       HigherPriorityAnchoredMessage_PreemptsPendingPmcMessage) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kDiscoveryNudge});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  blocking_handle.reset();
  ASSERT_TRUE(base::test::RunUntil([&]() { return !calls.empty(); }));

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorPmcTest,
       HigherPriorityChip_PreemptsPendingPmcMessage) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kDiscoveryNudge});
  selector->RequestChipShow(
      1, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 1)));
}

TEST_F(PriorityChipSelectorPmcTest, LowerPriorityIgnoredWhilePendingPmc) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kDiscoveryNudge});
  selector->RequestChipShow(
      2, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kDiscoveryNudge});

  EXPECT_THAT(calls, IsEmpty());

  blocking_handle.reset();
  ASSERT_TRUE(base::test::RunUntil([&]() { return !calls.empty(); }));

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(PriorityChipSelectorPmcTest,
       RequestAnchoredMessageHide_CancelsPendingPmc) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  selector->RequestAnchoredMessageHide(0);

  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);

  blocking_handle.reset();
  task_environment().FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calls, IsEmpty());
}

TEST_F(PriorityChipSelectorPmcTest,
       PrivacySecuritySecondAnchoredMessageDowngradedWhileFirstPending) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 1)));

  blocking_handle.reset();
  ASSERT_TRUE(base::test::RunUntil([&]() { return calls.size() == 2; }));

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 1),
                                 Pair("show_anchored_message", 0)));
}

TEST_F(PriorityChipSelectorPmcTest, PrivacySecurityAlongsideChips_WithPmc) {
  selector->RequestChipShow(
      0, SuggestionChipConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));

  selector->RequestAnchoredMessageShow(
      1, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});

  ASSERT_TRUE(base::test::RunUntil([&]() { return calls.size() == 2; }));
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0),
                                 Pair("show_anchored_message", 1)));
}

TEST_F(PriorityChipSelectorPmcTest,
       SameIdLowerPriorityAnchoredMessageReplacesPending) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  // Request higher priority anchored message for ID 0 while PMC is blocked.
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(calls, IsEmpty());

  // Request lower priority anchored message for the same ID 0.
  // Cancelling the pending higher-priority request should reset
  // active_priority_ and queue the lower-priority anchored message.
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  EXPECT_THAT(calls, IsEmpty());

  // Release the blocking notice and verify the lower-priority anchored message
  // shows.
  blocking_handle.reset();
  ASSERT_TRUE(base::test::RunUntil([&]() { return !calls.empty(); }));
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(PriorityChipSelectorPmcTest, SameIdChipReplacesPendingAnchoredMessage) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  // Request higher priority anchored message for ID 0 while PMC is blocked.
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{
             .priority = PageActionPriorityCategory::kPrivacySecurity});
  EXPECT_THAT(calls, IsEmpty());

  // Request chip show for the same ID 0 with lower priority.
  selector->RequestChipShow(
      0, SuggestionChipConfig{.priority =
                                  PageActionPriorityCategory::kContextualCue});

  // Chip shows immediately.
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));

  // Fast forward 10s: PMC timeout should not fire for the cancelled anchored
  // message.
  blocking_handle.reset();
  task_environment().FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorPmcTest,
       PendingPmcMessage_DowngradedOnTabDeactivation) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->OnTabActiveChanged(true);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  EXPECT_THAT(calls, IsEmpty());
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kWaiting);

  // Tab deactivation should cancel pending PMC message and downgrade to chip.
  selector->OnTabActiveChanged(false);
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);

  // Release blocking notice and fast forward: no further calls.
  blocking_handle.reset();
  task_environment().FastForwardBy(base::Seconds(10));
  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
}

TEST_F(PriorityChipSelectorPmcTest,
       ActivePmcMessage_DowngradedOnTabDeactivation) {
  selector->OnTabActiveChanged(true);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});
  ASSERT_TRUE(base::test::RunUntil([&]() { return !calls.empty(); }));
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kShowing);

  // Tab deactivation should hide anchored message, release PMC handle, and show
  // chip.
  selector->OnTabActiveChanged(false);
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0),
                                 Pair("hide_anchored_message", 0),
                                 Pair("show_chip", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);
}

TEST_F(PriorityChipSelectorPmcTest,
       AnchoredMessageRequestedOnInactiveTab_BypassesPmcAndShowsChip) {
  user_education::ProductMessagingHandle blocking_handle =
      AcquireBlockingNotice();

  selector->OnTabActiveChanged(false);
  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_chip", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);
}

TEST_F(PriorityChipSelectorPmcTest, FeatureDisabled_ShowsImmediately) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kPageActionsPrioritySelector,
                            features::kPageActionAnchoredMessageActiveTabOnly},
      /*disabled_features=*/{
          features::kPageActionsPrioritySelectorProductMessagingController});

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);
}

TEST_F(PriorityChipSelectorPmcTest, ActiveTabOnlyDisabled_ShowsImmediately) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {features::kPageActionsPrioritySelector,
       features::kPageActionsPrioritySelectorProductMessagingController},
      /*disabled_features=*/{
          features::kPageActionAnchoredMessageActiveTabOnly});

  selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
  EXPECT_EQ(pmc().GetMessageStatus(
                internal::PriorityChipSelector::kAnchoredMessageId),
            user_education::ProductMessageStatus::kNone);
}

TEST_F(PriorityChipSelectorPmcTest, NullPmc_ShowsImmediately) {
  auto null_pmc_selector = std::make_unique<internal::PriorityChipSelector>(
      base::BindRepeating(&PriorityChipSelectorPmcTest::ShowChipCallback,
                          base::Unretained(this)),
      base::BindRepeating(&PriorityChipSelectorPmcTest::HideChipCallback,
                          base::Unretained(this)),
      base::BindRepeating(
          &PriorityChipSelectorPmcTest::ShowAnchoredMessageCallback,
          base::Unretained(this)),
      base::BindRepeating(
          &PriorityChipSelectorPmcTest::HideAnchoredMessageCallback,
          base::Unretained(this)),
      nullptr);

  null_pmc_selector->RequestAnchoredMessageShow(
      0, AnchoredMessageConfig{.priority =
                                   PageActionPriorityCategory::kContextualCue});

  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

}  // namespace
}  // namespace page_actions
