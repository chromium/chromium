// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_anchored_message_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
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
    ON_CALL(model_, GetAnchoredMessageExpandableContent())
        .WillByDefault(ReturnRef(empty_expandable_content_));
    ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));
    ON_CALL(model_, GetText()).WillByDefault(ReturnRef(test_text_));
    ON_CALL(model_, GetAccessibleName()).WillByDefault(ReturnRef(empty_text_));
    ON_CALL(model_, GetTooltipText()).WillByDefault(ReturnRef(empty_text_));
  }

  void TearDown() override {
    anchor_widget_.reset();
    InteractiveViewsTestMixin::TearDown();
  }

  std::unique_ptr<views::Widget> CreateAnchoredMessageWidget() {
    auto view = std::make_unique<AnchoredMessageBubbleView>(
        views::BubbleAnchor(anchor_widget_->GetContentsView()), model_,
        delegate_);
    view->set_parent_window(anchor_widget_->GetNativeView());
    std::unique_ptr<views::Widget> widget =
        views::BubbleDialogDelegate::CreateBubble(std::move(view).release());
    widget->Show();
    return widget;
  }

 protected:
  NiceMock<MockPageActionModel> model_;
  NiceMock<MockAnchoredMessageDelegate> delegate_;
  std::optional<ui::ImageModel> no_icon_;
  std::optional<AnchoredMessageExpandableContent> empty_expandable_content_;
  std::optional<ui::ImageModel> test_icon_opt_ = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kInstallDesktopIcon
                                        : vector_icons::kInstallDesktopOldIcon);
  ui::ImageModel empty_image_;
  ui::ImageModel test_image_ = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kInstallDesktopIcon
                                        : vector_icons::kInstallDesktopOldIcon);
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

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageChipId,
                [this](views::LabelButton* chip) {
                  return chip->GetText() == test_text_ &&
                         chip->HasImage(views::Button::STATE_NORMAL);
                }),
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
  ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(empty_image_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kClose));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageChipId,
                [this](views::LabelButton* chip) {
                  return chip->GetText() == test_text_ &&
                         !chip->HasImage(views::Button::STATE_NORMAL);
                }),
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
  ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(empty_image_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kMenu));
  ON_CALL(model_, GetAnchoredMessageMenuModel())
      .WillByDefault(Return(&menu_model));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageChipId,
                [this](views::LabelButton* chip) {
                  return chip->GetText() == test_text_ &&
                         !chip->HasImage(views::Button::STATE_NORMAL);
                }),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest,
       UpdateContentChangesVisibility_AllVisible_NullMenuModel) {
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kMenu));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageChipId,
                [this](views::LabelButton* chip) {
                  return chip->GetText() == test_text_ &&
                         chip->HasImage(views::Button::STATE_NORMAL);
                }),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest, UpdateContentChangesVisibility_ChipOnly) {
  ON_CALL(model_, GetText()).WillByDefault(ReturnRef(empty_text_));
  ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageLabelId),
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageChipId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageChipId,
                [](views::LabelButton* chip) {
                  return chip->GetText().empty() &&
                         chip->HasImage(views::Button::STATE_NORMAL);
                }),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      EnsureNotPresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest, CloseButtonIsFocusable) {
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kClose));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId,
                [](views::View* button) { return button->IsFocusable(); }));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest, MenuButtonIsFocusable) {
  ui::SimpleMenuModel menu_model(nullptr);
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kMenu));
  ON_CALL(model_, GetAnchoredMessageMenuModel())
      .WillByDefault(Return(&menu_model));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId,
                [](views::View* button) { return button->IsFocusable(); }));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest, ExpandButtonFocusRing) {
  std::optional<AnchoredMessageExpandableContent> expandable_content =
      std::make_optional<AnchoredMessageExpandableContent>();
  expandable_content->items.push_back({test_image_, test_text_});

  ON_CALL(model_, GetAnchoredMessageExpandableContent())
      .WillByDefault(ReturnRef(expandable_content));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      WithView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
               [](views::Button* button) { button->RequestFocus(); }),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [](views::Button* button) {
                  EXPECT_TRUE(button->HasFocus());
                  views::FocusRing* focus_ring = views::FocusRing::Get(button);
                  EXPECT_NE(focus_ring, nullptr);
                  EXPECT_TRUE(focus_ring->ShouldPaintForTesting());
                  return true;
                }));

  widget->CloseNow();
}

TEST_F(AnchoredMessageBubbleViewTest, ExpandButtonTooltip) {
  std::optional<AnchoredMessageExpandableContent> expandable_content =
      std::make_optional<AnchoredMessageExpandableContent>();
  expandable_content->items.push_back({test_image_, test_text_});

  ON_CALL(model_, GetAnchoredMessageExpandableContent())
      .WillByDefault(ReturnRef(expandable_content));

  std::unique_ptr<views::Widget> widget = CreateAnchoredMessageWidget();

  const std::u16string custom_expand_tooltip = u"Custom expand tooltip";
  const std::u16string custom_collapse_tooltip = u"Custom collapse tooltip";

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [](views::Button* button) {
                  return button->GetTooltipText() ==
                             l10n_util::GetStringUTF16(
                                 IDS_ANCHORED_MESSAGE_EXPAND_BUTTON_TOOLTIP) &&
                         button->GetViewAccessibility().GetCachedName() ==
                             l10n_util::GetStringUTF16(
                                 IDS_ANCHORED_MESSAGE_EXPAND_BUTTON_TOOLTIP);
                }),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckView(
          AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
          [](views::Button* button) {
            return button->GetTooltipText() ==
                       l10n_util::GetStringUTF16(
                           IDS_ANCHORED_MESSAGE_COLLAPSE_BUTTON_TOOLTIP) &&
                   button->GetViewAccessibility().GetCachedName() ==
                       l10n_util::GetStringUTF16(
                           IDS_ANCHORED_MESSAGE_COLLAPSE_BUTTON_TOOLTIP);
          }),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [](views::Button* button) {
                  return button->GetTooltipText() ==
                             l10n_util::GetStringUTF16(
                                 IDS_ANCHORED_MESSAGE_EXPAND_BUTTON_TOOLTIP) &&
                         button->GetViewAccessibility().GetCachedName() ==
                             l10n_util::GetStringUTF16(
                                 IDS_ANCHORED_MESSAGE_EXPAND_BUTTON_TOOLTIP);
                }),
      WithView(AnchoredMessageBubbleView::kAnchoredMessageBubbleId,
               [this, &expandable_content, &custom_expand_tooltip,
                &custom_collapse_tooltip](views::View* view) {
                 auto* bubble_view =
                     static_cast<AnchoredMessageBubbleView*>(view);
                 expandable_content->expand_button_tooltip =
                     custom_expand_tooltip;
                 expandable_content->collapse_button_tooltip =
                     custom_collapse_tooltip;
                 bubble_view->UpdateContent(model_);
               }),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [&custom_expand_tooltip](views::Button* button) {
                  return button->GetTooltipText() == custom_expand_tooltip &&
                         button->GetViewAccessibility().GetCachedName() ==
                             custom_expand_tooltip;
                }),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [&custom_collapse_tooltip](views::Button* button) {
                  return button->GetTooltipText() == custom_collapse_tooltip &&
                         button->GetViewAccessibility().GetCachedName() ==
                             custom_collapse_tooltip;
                }),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [&custom_expand_tooltip](views::Button* button) {
                  return button->GetTooltipText() == custom_expand_tooltip &&
                         button->GetViewAccessibility().GetCachedName() ==
                             custom_expand_tooltip;
                }));

  widget->CloseNow();
}
}  // namespace page_actions
