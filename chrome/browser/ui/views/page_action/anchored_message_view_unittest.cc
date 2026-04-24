// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_anchored_message_delegate.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace page_actions {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::views::test::InteractiveViewsTestMixin;

class AnchoredMessageBubbleViewTest
    : public InteractiveViewsTestMixin<ChromeViewsTestBase> {
 public:
  AnchoredMessageBubbleViewTest() = default;
  ~AnchoredMessageBubbleViewTest() override = default;

  void SetUp() override {
    InteractiveViewsTestMixin::SetUp();

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
    SetContextWidget(anchor_widget_.get());

    // Setup model defaults.
    ON_CALL(model_, GetAnchoredMessageIcon())
        .WillByDefault(ReturnRef(no_icon_));
    ON_CALL(model_, GetAnchoredMessageText())
        .WillByDefault(ReturnRef(empty_text_));
    ON_CALL(model_, GetAnchoredMessageActionIconType())
        .WillByDefault(Return(AnchoredMessageActionIconType::kNone));
    ON_CALL(model_, GetAnchoredMessageMenuModel())
        .WillByDefault(Return(nullptr));
    ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));
    ON_CALL(model_, GetText()).WillByDefault(ReturnRef(test_text_));
  }

  void TearDown() override {
    anchor_widget_.reset();
    InteractiveViewsTestMixin::TearDown();
  }

  std::unique_ptr<AnchoredMessageBubbleView> CreateView() {
    auto view = std::make_unique<AnchoredMessageBubbleView>(
        views::BubbleAnchor(anchor_widget_->GetContentsView()), model_,
        delegate_);
    view->set_parent_window(anchor_widget_->GetNativeView());
    return view;
  }

 protected:
  NiceMock<MockPageActionModel> model_;
  NiceMock<MockAnchoredMessageDelegate> delegate_;
  std::optional<ui::ImageModel> no_icon_ = std::nullopt;
  std::optional<ui::ImageModel> test_icon_opt_ =
      ui::ImageModel::FromVectorIcon(vector_icons::kInstallDesktopIcon);
  ui::ImageModel empty_image_;
  ui::ImageModel test_image_ =
      ui::ImageModel::FromVectorIcon(vector_icons::kInstallDesktopIcon);
  std::u16string empty_text_ = u"";
  std::u16string test_text_ = u"Test text";

  base::test::ScopedRunLoopTimeout timeout_{FROM_HERE, base::Seconds(10)};
  std::unique_ptr<views::Widget> anchor_widget_;
};

TEST_F(AnchoredMessageBubbleViewTest, VisibilityReflectsModelOnCreation) {
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kClose));

  auto view = CreateView();
  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(view));
  widget->Show();

  RunTestSequence(
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest,
       UpdateContentChangesVisibility_AllVisible_CloseIcon) {
  ON_CALL(model_, GetAnchoredMessageIcon())
      .WillByDefault(ReturnRef(test_icon_opt_));
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetText()).WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kClose));

  auto view = CreateView();
  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(view));
  widget->Show();

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageChipIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest,
       UpdateContentChangesVisibility_AllVisible_MenuIcon) {
  ui::SimpleMenuModel menu_model(nullptr);
  ON_CALL(model_, GetAnchoredMessageIcon())
      .WillByDefault(ReturnRef(test_icon_opt_));
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetText()).WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kMenu));
  ON_CALL(model_, GetAnchoredMessageMenuModel())
      .WillByDefault(Return(&menu_model));

  auto view = CreateView();
  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(view));
  widget->Show();

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageChipIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipLabelId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest,
       UpdateContentChangesVisibility_AllVisible_NullMenuModel) {
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kMenu));

  auto view = CreateView();
  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(view));
  widget->Show();

  RunTestSequence(
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipLabelId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest, UpdateContentChangesVisibility_ChipOnly) {
  ON_CALL(model_, GetText()).WillByDefault(ReturnRef(empty_text_));
  ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));

  auto view = CreateView();
  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(view));
  widget->Show();

  RunTestSequence(
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageChipLabelId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

}  // namespace page_actions
