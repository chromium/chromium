// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/chip_selector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"

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
                            base::Unretained(this)));
  }

 private:
  void ShowChipCallback(actions::ActionId page_action_id,
                        const SuggestionChipConfig& config) {
    calls.emplace_back("show", page_action_id);
  }

  void HideChipCallback(actions::ActionId page_action_id) {
    calls.emplace_back("hide", page_action_id);
  }

 public:
  std::unique_ptr<internal::DefaultChipSelector> selector;
  std::vector<std::pair<std::string, actions::ActionId>> calls;
};

TEST_F(DefaultChipSelectorTest, ShowSingleChip) {
  selector->RequestChipShow(1, SuggestionChipConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show", 1)));
}

TEST_F(DefaultChipSelectorTest, ShowChipTwice) {
  selector->RequestChipShow(2, SuggestionChipConfig{});
  selector->RequestChipShow(2, SuggestionChipConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show", 2), Pair("show", 2)));
}

TEST_F(DefaultChipSelectorTest, ShowTwoChips) {
  selector->RequestChipShow(3, SuggestionChipConfig{});
  selector->RequestChipShow(4, SuggestionChipConfig{});
  EXPECT_THAT(calls, ElementsAre(Pair("show", 3), Pair("show", 4)));
}

TEST_F(DefaultChipSelectorTest, HideUnshownChip) {
  selector->RequestChipHide(10);
  EXPECT_THAT(calls, ElementsAre(Pair("hide", 10)));
}

TEST_F(DefaultChipSelectorTest, HideShownChip) {
  selector->RequestChipShow(11, SuggestionChipConfig{});
  selector->RequestChipHide(11);
  EXPECT_THAT(calls, ElementsAre(Pair("show", 11), Pair("hide", 11)));
}

}  // namespace
}  // namespace page_actions
