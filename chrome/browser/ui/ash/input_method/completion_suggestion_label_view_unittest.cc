// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/completion_suggestion_label_view.h"

#include <stddef.h>

#include <string>

#include "chrome/browser/ui/ash/input_method/colors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

namespace ui {
namespace ime {
namespace {

// Returns the child of `view` at `index` as a views::Label.
views::Label* LabelAt(const views::View& view, size_t index) {
  views::View* const child = view.children()[index];
  EXPECT_TRUE(views::IsViewClass<views::Label>(child));
  return static_cast<views::Label*>(child);
}

class CompletionSuggestionLabelViewTest : public views::ViewsTestBase {
 public:
  CompletionSuggestionLabelViewTest() = default;
};

TEST_F(CompletionSuggestionLabelViewTest, EmptyPrefixHasCorrectText) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"", u"good");

  EXPECT_EQ(label.GetText(), u"good");
}

TEST_F(CompletionSuggestionLabelViewTest, NonEmptyPrefixHasCorrectText) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"how a", u"re you");

  EXPECT_EQ(label.GetText(), u"how are you");
}

TEST_F(CompletionSuggestionLabelViewTest,
       ChildHasCorrectTextWhenPrefixIsEmpty) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"", u"good");

  ASSERT_EQ(label.children().size(), 1u);
  EXPECT_EQ(LabelAt(label, 0)->GetText(), u"good");
}

TEST_F(CompletionSuggestionLabelViewTest,
       ChildUsesSecondaryColorWhenPrefixIsEmpty) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"", u"good");

  ASSERT_EQ(label.children().size(), 1u);
  EXPECT_EQ(LabelAt(label, 0)->GetEnabledColor(),
            ResolveSemanticColor(cros_styles::ColorName::kTextColorSecondary));
}

TEST_F(CompletionSuggestionLabelViewTest,
       ChildrenHaveCorrectTextWhenPrefixIsNotEmpty) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"how a", u"re you");

  ASSERT_EQ(label.children().size(), 2u);
  EXPECT_EQ(LabelAt(label, 0)->GetText(), u"how a");
  EXPECT_EQ(LabelAt(label, 1)->GetText(), u"re you");
}

TEST_F(CompletionSuggestionLabelViewTest,
       ChildrenUsePrimaryAndSecondaryColorsWhenPrefixIsNotEmpty) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"how a", u"re you");

  ASSERT_EQ(label.children().size(), 2u);
  EXPECT_EQ(LabelAt(label, 0)->GetEnabledColor(),
            ResolveSemanticColor(cros_styles::ColorName::kTextColorPrimary));
  EXPECT_EQ(LabelAt(label, 1)->GetEnabledColor(),
            ResolveSemanticColor(cros_styles::ColorName::kTextColorSecondary));
}

TEST_F(CompletionSuggestionLabelViewTest, PrefixWidthIsZeroWhenPrefixIsEmpty) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"", u"good");

  EXPECT_EQ(label.GetPrefixWidthPx(), 0);
}

TEST_F(CompletionSuggestionLabelViewTest,
       PrefixWidthIsWidthOfFirstChildWhenPrefixIsNotEmpty) {
  CompletionSuggestionLabelView label;

  label.SetPrefixAndPrediction(u"how a", u"re you");

  ASSERT_FALSE(label.children().empty());
  EXPECT_EQ(label.GetPrefixWidthPx(), LabelAt(label, 0)->width());
}

}  // namespace
}  // namespace ime
}  // namespace ui
