// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/completion_suggestion_view.h"

#include <stddef.h>

#include <string>

#include "chrome/browser/ui/ash/input_method/completion_suggestion_label_view.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace ui {
namespace ime {
namespace {

class CompletionSuggestionViewTest : public views::ViewsTestBase {
 public:
  CompletionSuggestionViewTest() = default;
};

TEST_F(CompletionSuggestionViewTest,
       AnchorOriginIsPaddingWhenConfirmedLengthIsZero) {
  CompletionSuggestionView suggestion({});
  suggestion.SetView({
      .text = u"good",
      .confirmed_length = 0,
  });

  EXPECT_EQ(suggestion.GetAnchorOrigin(), gfx::Point(kPadding, 0));
}

TEST_F(CompletionSuggestionViewTest,
       AnchorOriginIsPaddingAndPrefixWidthWhenConfirmedLengthIsNonZero) {
  CompletionSuggestionView suggestion({});
  // "how a" is confirmed
  suggestion.SetView({
      .text = u"how are you",
      .confirmed_length = 5,
  });

  EXPECT_EQ(
      suggestion.GetAnchorOrigin(),
      gfx::Point(
          kPadding +
              suggestion.suggestion_label_for_testing()->GetPrefixWidthPx(),
          0));
}

}  // namespace
}  // namespace ime
}  // namespace ui
