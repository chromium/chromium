// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/chip_selector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
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

  void ShowAnchoredMessageCallback(actions::ActionId page_action_id) {
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
  selector->RequestAnchoredMessageShow(0);
  EXPECT_THAT(
      calls, ElementsAre(Pair("show_chip", 0), Pair("show_anchored_message", 0),
                         Pair("hide_chip", 0)));
}

TEST_F(DefaultChipSelectorTest, ChipHidesAnchoredMessage) {
  selector->RequestAnchoredMessageShow(0);
  selector->RequestChipShow(0, SuggestionChipConfig{});
  EXPECT_THAT(
      calls, ElementsAre(Pair("show_anchored_message", 0), Pair("show_chip", 0),
                         Pair("hide_anchored_message", 0)));
}

TEST_F(DefaultChipSelectorTest, OnlyFirstAnchoredMessageShows) {
  selector->RequestAnchoredMessageShow(0);
  selector->RequestAnchoredMessageShow(1);
  EXPECT_THAT(calls, ElementsAre(Pair("show_anchored_message", 0)));
}

TEST_F(DefaultChipSelectorTest, AnchoredMessageQueue) {
  selector->RequestAnchoredMessageShow(0);
  selector->RequestAnchoredMessageShow(1);
  selector->RequestAnchoredMessageShow(2);
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

}  // namespace
}  // namespace page_actions
