// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/grammar_suggestion_window.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "chrome/browser/ui/ash/input_method/mock_assistive_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ime {

class GrammarSuggestionWindowTest : public ChromeViewsTestBase {
 public:
  GrammarSuggestionWindowTest() {}
  ~GrammarSuggestionWindowTest() override {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    grammar_suggestion_window_ =
        new GrammarSuggestionWindow(GetContext(), delegate_.get());
    suggestion_button_.id = ButtonId::kSuggestion;
    ignore_button_.id = ButtonId::kIgnoreSuggestion;
    grammar_suggestion_window_->InitWidget();
  }

  void TearDown() override {
    grammar_suggestion_window_->GetWidget()->CloseNow();
    ChromeViewsTestBase::TearDown();
  }

  raw_ptr<GrammarSuggestionWindow, DanglingUntriaged>
      grammar_suggestion_window_;
  std::unique_ptr<MockAssistiveDelegate> delegate_ =
      std::make_unique<MockAssistiveDelegate>();
  AssistiveWindowButton suggestion_button_;
  AssistiveWindowButton ignore_button_;
};

TEST_F(GrammarSuggestionWindowTest, HighlightsSuggestionButton) {
  grammar_suggestion_window_->Show();
  grammar_suggestion_window_->SetButtonHighlighted(suggestion_button_, true);

  EXPECT_NE(
      grammar_suggestion_window_->GetSuggestionButtonForTesting()->background(),
      nullptr);
  EXPECT_EQ(
      grammar_suggestion_window_->GetIgnoreButtonForTesting()->background(),
      nullptr);
}

TEST_F(GrammarSuggestionWindowTest, HighlightsIgnoreButton) {
  grammar_suggestion_window_->Show();
  grammar_suggestion_window_->SetButtonHighlighted(ignore_button_, true);

  EXPECT_NE(
      grammar_suggestion_window_->GetIgnoreButtonForTesting()->background(),
      nullptr);
  EXPECT_EQ(
      grammar_suggestion_window_->GetSuggestionButtonForTesting()->background(),
      nullptr);
}

TEST_F(GrammarSuggestionWindowTest,
       HighlightsSuggestionButtonThenIgnoreButton) {
  grammar_suggestion_window_->Show();
  grammar_suggestion_window_->SetButtonHighlighted(suggestion_button_, true);
  grammar_suggestion_window_->SetButtonHighlighted(ignore_button_, true);

  EXPECT_NE(
      grammar_suggestion_window_->GetIgnoreButtonForTesting()->background(),
      nullptr);
  EXPECT_EQ(
      grammar_suggestion_window_->GetSuggestionButtonForTesting()->background(),
      nullptr);
}

TEST_F(GrammarSuggestionWindowTest,
       HighlightsIgnoreButtonThenSuggestionButton) {
  grammar_suggestion_window_->Show();
  grammar_suggestion_window_->SetButtonHighlighted(ignore_button_, true);
  grammar_suggestion_window_->SetButtonHighlighted(suggestion_button_, true);

  EXPECT_NE(
      grammar_suggestion_window_->GetSuggestionButtonForTesting()->background(),
      nullptr);
  EXPECT_EQ(
      grammar_suggestion_window_->GetIgnoreButtonForTesting()->background(),
      nullptr);
}

TEST_F(GrammarSuggestionWindowTest, UnhighlightsSuggestionButton) {
  grammar_suggestion_window_->Show();
  grammar_suggestion_window_->SetButtonHighlighted(suggestion_button_, true);
  grammar_suggestion_window_->SetButtonHighlighted(suggestion_button_, false);

  EXPECT_EQ(
      grammar_suggestion_window_->GetSuggestionButtonForTesting()->background(),
      nullptr);
  EXPECT_EQ(
      grammar_suggestion_window_->GetIgnoreButtonForTesting()->background(),
      nullptr);
}

TEST_F(GrammarSuggestionWindowTest, UnhighlightsIgnoreButton) {
  grammar_suggestion_window_->Show();
  grammar_suggestion_window_->SetButtonHighlighted(ignore_button_, true);
  grammar_suggestion_window_->SetButtonHighlighted(ignore_button_, false);

  EXPECT_EQ(
      grammar_suggestion_window_->GetIgnoreButtonForTesting()->background(),
      nullptr);
  EXPECT_EQ(
      grammar_suggestion_window_->GetSuggestionButtonForTesting()->background(),
      nullptr);
}

TEST_F(GrammarSuggestionWindowTest, SetsSuggestion) {
  std::u16string test_suggestion = u"test suggestion";
  grammar_suggestion_window_->Show();
  grammar_suggestion_window_->SetSuggestion(test_suggestion);

  EXPECT_EQ(grammar_suggestion_window_->GetSuggestionButtonForTesting()
                ->GetSuggestionForTesting(),
            test_suggestion);
}

}  // namespace ime
}  // namespace ui
