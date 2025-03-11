// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_CUSTOM_HELP_BUBBLE_VIEW_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_CUSTOM_HELP_BUBBLE_VIEW_H_

#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace user_education::test {

class TestCustomHelpBubbleView : public views::BubbleDialogDelegateView,
                                 public CustomHelpBubbleUi {
  METADATA_HEADER(TestCustomHelpBubbleView, views::BubbleDialogDelegateView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBubbleId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDismissButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kActionButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSnoozeButtonId);

  TestCustomHelpBubbleView(views::View* anchor_view,
                           views::BubbleBorder::Arrow arrow);
  ~TestCustomHelpBubbleView() override;

  auto* cancel_button() { return cancel_button_.get(); }
  auto* dismiss_button() { return dismiss_button_.get(); }
  auto* action_button() { return action_button_.get(); }
  auto* snooze_button() { return snooze_button_.get(); }

 private:
  raw_ptr<views::LabelButton> cancel_button_;
  raw_ptr<views::LabelButton> dismiss_button_;
  raw_ptr<views::LabelButton> action_button_;
  raw_ptr<views::LabelButton> snooze_button_;
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_CUSTOM_HELP_BUBBLE_VIEW_H_
