// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/suggestion_window_view.h"

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "chrome/browser/ui/ash/input_method/completion_suggestion_label_view.h"
#include "chrome/browser/ui/ash/input_method/completion_suggestion_view.h"
#include "chrome/browser/ui/ash/input_method/mock_assistive_delegate.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/button_test_api.h"

namespace ui {
namespace ime {

class SuggestionWindowViewTest
    : public ChromeViewsTestBase,
      public ::testing::WithParamInterface<SuggestionWindowView::Orientation> {
 public:
  SuggestionWindowViewTest() {}

  SuggestionWindowViewTest(const SuggestionWindowViewTest&) = delete;
  SuggestionWindowViewTest& operator=(const SuggestionWindowViewTest&) = delete;

  ~SuggestionWindowViewTest() override {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    InitCandidates();
    window_.candidates = candidates_;
    window_.show_setting_link = true;

    suggestion_window_view_ =
        SuggestionWindowView::Create(GetContext(), delegate_.get(), GetParam());
    candidate_button_.id = ButtonId::kSuggestion;
    setting_link_view_.id = ButtonId::kSmartInputsSettingLink;
    learn_more_button_.id = ButtonId::kLearnMore;
  }

  void TearDown() override {
    suggestion_window_view_->GetWidget()->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  void InitCandidates() {
    for (int i = 0; i < 3; i++) {
      std::string candidate = base::NumberToString(i);
      candidates_.push_back(base::UTF8ToUTF16(candidate));
    }
  }

  size_t GetHighlightedCount() const {
    const auto& children =
        suggestion_window_view_->multiple_candidate_area_for_testing()
            ->children();
    return base::ranges::count_if(
        children, [](const views::View* v) { return !!v->background(); });
  }

  std::optional<int> GetHighlightedIndex() const {
    const auto& children =
        suggestion_window_view_->multiple_candidate_area_for_testing()
            ->children();
    const auto it = base::ranges::find_if(
        children, [](const views::View* v) { return !!v->background(); });
    return (it == children.cend())
               ? std::nullopt
               : std::make_optional(std::distance(children.cbegin(), it));
  }

  raw_ptr<SuggestionWindowView, DanglingUntriaged> suggestion_window_view_;
  std::unique_ptr<MockAssistiveDelegate> delegate_ =
      std::make_unique<MockAssistiveDelegate>();
  std::vector<std::u16string> candidates_;
  ash::input_method::AssistiveWindowProperties window_;
  AssistiveWindowButton candidate_button_;
  AssistiveWindowButton setting_link_view_;
  AssistiveWindowButton learn_more_button_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SuggestionWindowViewTest,
    testing::Values(SuggestionWindowView::Orientation::kHorizontal,
                    SuggestionWindowView::Orientation::kVertical),
    // Function to make the test name say ".../kHorizontal" etc.
    [](const testing::TestParamInfo<SuggestionWindowViewTest::ParamType>&
           info) {
      std::string name;
      switch (info.param) {
        case SuggestionWindowView::Orientation::kHorizontal:
          name = "InitInHorizontal";
          break;
        case SuggestionWindowView::Orientation::kVertical:
          name = "InitInVertical";
          break;
        default:
          name = "UNKNOWN";
          break;
      }
      return name;
    });

TEST_P(SuggestionWindowViewTest, HighlightOneCandidateWhenIndexIsValid) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  for (int index = 0; index < static_cast<int>(candidates_.size()); index++) {
    candidate_button_.suggestion_index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(index, GetHighlightedIndex());
  }
}

TEST_P(SuggestionWindowViewTest, HighlightNoCandidateWhenIndexIsInvalid) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  for (int index : {-1, static_cast<int>(candidates_.size())}) {
    candidate_button_.suggestion_index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(0u, GetHighlightedCount());
    EXPECT_FALSE(GetHighlightedIndex().has_value());
  }
}

TEST_P(SuggestionWindowViewTest, HighlightTheSameCandidateWhenCalledTwice) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  int highlight_index = 0;
  candidate_button_.suggestion_index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(highlight_index, GetHighlightedIndex());
}

TEST_P(SuggestionWindowViewTest,
       HighlightValidCandidateAfterGivingInvalidIndexThenValidIndex) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  int valid_index = 0;
  candidate_button_.suggestion_index = candidates_.size();
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.suggestion_index = valid_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(valid_index, GetHighlightedIndex());
}

TEST_P(SuggestionWindowViewTest,
       KeepHighlightingValidCandidateWhenGivingValidThenInvalidIndex) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  int valid_index = 0;
  candidate_button_.suggestion_index = valid_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.suggestion_index = candidates_.size();
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(valid_index, GetHighlightedIndex());
}

TEST_P(SuggestionWindowViewTest, UnhighlightCandidateIfCurrentlyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  candidate_button_.suggestion_index = 0;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

  EXPECT_EQ(0u, GetHighlightedCount());
  EXPECT_FALSE(GetHighlightedIndex().has_value());
}

TEST_P(SuggestionWindowViewTest,
       DoesNotUnhighlightCandidateIfNotCurrentlyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  int highlight_index = 0;
  candidate_button_.suggestion_index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.suggestion_index = -1;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(highlight_index, GetHighlightedIndex());
}

TEST_P(SuggestionWindowViewTest, DoesNotUnhighlightCandidateIfOutOfRange) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  int highlight_index = 0;
  candidate_button_.suggestion_index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  for (int index : {-1, static_cast<int>(candidates_.size())}) {
    candidate_button_.suggestion_index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(highlight_index, GetHighlightedIndex());
  }
}

TEST_P(SuggestionWindowViewTest, HighlightsSettingLinkViewWhenNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       HighlightsSettingLinkViewWhenAlreadyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, true);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest, UnhighlightsSettingLinkViewWhenHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       UnhighlightsKeepSettingLinkViewUnhighlightedWhenAlreadyNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);
  suggestion_window_view_->SetButtonHighlighted(setting_link_view_, false);

  EXPECT_TRUE(
      suggestion_window_view_->setting_link_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest, HighlightsLearnMoreButtonWhenNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       HighlightsLearnMoreButtonWhenAlreadyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, true);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() !=
      nullptr);
}

TEST_P(SuggestionWindowViewTest, UnhighlightsLearnMoreButtonWhenHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest,
       UnhighlightsKeepLearnMoreButtonUnhighlightedWhenAlreadyNotHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);
  suggestion_window_view_->SetButtonHighlighted(learn_more_button_, false);

  EXPECT_TRUE(
      suggestion_window_view_->learn_more_button_for_testing()->background() ==
      nullptr);
}

TEST_P(SuggestionWindowViewTest, SetUpInCorrectOrientationLayoutOnInit) {
  views::BoxLayout::Orientation expected_orientation;
  switch (GetParam()) {
    case SuggestionWindowView::Orientation::kHorizontal:
      expected_orientation = views::BoxLayout::Orientation::kHorizontal;
      break;
    case SuggestionWindowView::Orientation::kVertical:
      expected_orientation = views::BoxLayout::Orientation::kVertical;
      break;
    default:
      abort();
  }

  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          suggestion_window_view_->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, expected_orientation);
}

TEST_P(SuggestionWindowViewTest, HasVerticalLayoutWhenShowSingleCandidate) {
  suggestion_window_view_->Show({
      .text = u"good",
      .confirmed_length = 0,
  });

  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          suggestion_window_view_->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kVertical);
}

TEST_P(SuggestionWindowViewTest,
       HasHorizontalLayoutWhenShowMultipleCandidateWithHorizontal) {
  suggestion_window_view_->ShowMultipleCandidates(
      window_, SuggestionWindowView::Orientation::kHorizontal);

  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          suggestion_window_view_->GetLayoutManager())
          ->GetOrientation();
  views::BoxLayout::Orientation candidate_area_layout_orientation =
      static_cast<views::BoxLayout*>(
          suggestion_window_view_->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kHorizontal);
  EXPECT_EQ(candidate_area_layout_orientation,
            views::BoxLayout::Orientation::kHorizontal);
}

TEST_P(SuggestionWindowViewTest,
       HasVerticalLayoutWhenShowMultipleCandidateWithVertical) {
  suggestion_window_view_->ShowMultipleCandidates(
      window_, SuggestionWindowView::Orientation::kVertical);

  views::BoxLayout::Orientation layout_orientation =
      static_cast<views::BoxLayout*>(
          suggestion_window_view_->GetLayoutManager())
          ->GetOrientation();
  views::BoxLayout::Orientation candidate_area_layout_orientation =
      static_cast<views::BoxLayout*>(
          suggestion_window_view_->multiple_candidate_area_for_testing()
              ->GetLayoutManager())
          ->GetOrientation();
  EXPECT_EQ(layout_orientation, views::BoxLayout::Orientation::kVertical);
  EXPECT_EQ(candidate_area_layout_orientation,
            views::BoxLayout::Orientation::kVertical);
}

TEST_P(SuggestionWindowViewTest,
       LeftBoundIsCloseToAnchorWithNoConfirmedLength) {
  suggestion_window_view_->Show({
      .text = u"good",
      .confirmed_length = 0,
  });

  suggestion_window_view_->SetAnchorRect(gfx::Rect(100, 0, 10, 10));

  EXPECT_EQ(suggestion_window_view_->GetBoundsInScreen().x(), 100 - kPadding);
}

TEST_P(SuggestionWindowViewTest,
       LeftBoundIsOffsetFromAnchorWithConfirmedLength) {
  // "how a" is confirmed
  suggestion_window_view_->Show({
      .text = u"how are you",
      .confirmed_length = 5,
  });

  suggestion_window_view_->SetAnchorRect(gfx::Rect(100, 0, 10, 10));

  // The right border of the confirmed part "how a" must align with the left
  // border of the anchor rect.
  const gfx::FontList font_list(
      {CompletionSuggestionLabelView::kFontName}, gfx::Font::NORMAL,
      CompletionSuggestionLabelView::kFontSize, gfx::Font::Weight::NORMAL);
  EXPECT_EQ(suggestion_window_view_->GetBoundsInScreen().x(),
            100 - kPadding - gfx::GetStringWidth(u"how a", font_list));
}

TEST_P(SuggestionWindowViewTest,
       SendsCorrectWindowTypeForLearnMoreButtonClick) {
  window_.type = ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion;
  suggestion_window_view_->ShowMultipleCandidates(window_, GetParam());
  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), ui::EF_NONE,
                             ui::EF_NONE);

  views::test::ButtonTestApi(
      suggestion_window_view_->learn_more_button_for_testing())
      .NotifyClick(mouse_event);

  EXPECT_EQ(MockAssistiveDelegate::last_window_type_,
            ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion);
}

}  // namespace ime
}  // namespace ui
