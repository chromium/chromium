// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_custom_help_bubble_view.h"

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace user_education::test {

TestCustomHelpBubbleView::TestCustomHelpBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : views::BubbleDialogDelegateView(anchor_view, arrow) {
  SetProperty(views::kElementIdentifierKey, kBubbleId);

  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  cancel_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindOnce(&TestCustomHelpBubbleView::NotifyUserAction,
                     base::Unretained(this), UserAction::kCancel),
      u"Cancel"));
  cancel_button_->SetProperty(views::kElementIdentifierKey, kCancelButtonId);
  dismiss_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindOnce(&TestCustomHelpBubbleView::NotifyUserAction,
                     base::Unretained(this), UserAction::kDismiss),
      u"Dismiss"));
  dismiss_button_->SetProperty(views::kElementIdentifierKey, kDismissButtonId);
  action_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindOnce(&TestCustomHelpBubbleView::NotifyUserAction,
                     base::Unretained(this), UserAction::kAction),
      u"Action"));
  action_button_->SetProperty(views::kElementIdentifierKey, kActionButtonId);
  snooze_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindOnce(&TestCustomHelpBubbleView::NotifyUserAction,
                     base::Unretained(this), UserAction::kSnooze),
      u"Snooze"));
  snooze_button_->SetProperty(views::kElementIdentifierKey, kSnoozeButtonId);
}

TestCustomHelpBubbleView::~TestCustomHelpBubbleView() {
  cancel_button_ = nullptr;
  dismiss_button_ = nullptr;
  action_button_ = nullptr;
  snooze_button_ = nullptr;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TestCustomHelpBubbleView, kBubbleId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TestCustomHelpBubbleView,
                                      kCancelButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TestCustomHelpBubbleView,
                                      kDismissButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TestCustomHelpBubbleView,
                                      kActionButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TestCustomHelpBubbleView,
                                      kSnoozeButtonId);

BEGIN_METADATA(TestCustomHelpBubbleView)
END_METADATA

}  // namespace user_education::test
