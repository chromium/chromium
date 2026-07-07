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
#include "ui/views/controls/image_view.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
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

    CreateAnchoredMessageWidget();
  }

  void TearDown() override {
    bubble_view_ = nullptr;
    bubble_widget_.reset();
    anchor_widget_.reset();
    InteractiveViewsTestMixin::TearDown();
  }

  void CreateAnchoredMessageWidget() {
    auto view = std::make_unique<AnchoredMessageBubbleView>(
        views::BubbleAnchor(anchor_widget_->GetContentsView()), model_,
        delegate_);
    bubble_view_ = view.get();
    view->set_parent_window(anchor_widget_->GetNativeView());
    bubble_widget_ =
        views::BubbleDialogDelegate::CreateBubble(std::move(view).release());
    bubble_widget_->Show();
  }

  auto CheckButtonTooltip(std::u16string expected_tooltip) {
    return CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                     [expected_tooltip](views::Button* button) {
                       return button->GetTooltipText() == expected_tooltip;
                     });
  }

  auto CheckAccessibility(std::u16string expected_accessible_name,
                          std::string expected_description,
                          bool expected_expanded) {
    return CheckView(
        AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
        [expected_accessible_name, expected_description,
         expected_expanded](views::Button* button) {
          ui::AXNodeData node_data;
          button->GetViewAccessibility().GetAccessibleNodeData(&node_data);
          std::string description = node_data.GetStringAttribute(
              ax::mojom::StringAttribute::kDescription);
          return button->GetViewAccessibility().GetCachedName() ==
                     expected_accessible_name &&
                 node_data.role == ax::mojom::Role::kButton &&
                 node_data.HasState(ax::mojom::State::kExpanded) ==
                     expected_expanded &&
                 node_data.HasState(ax::mojom::State::kCollapsed) ==
                     !expected_expanded &&
                 description == expected_description;
        });
  }

  static void CollectImageViews(views::View* view,
                                std::vector<views::ImageView*>& out) {
    if (auto* img = views::AsViewClass<views::ImageView>(view)) {
      out.push_back(img);
    }
    for (views::View* child : view->children()) {
      CollectImageViews(child, out);
    }
  }

  auto CheckButtonVectorIcon(const gfx::VectorIcon* expected_icon) {
    return CheckView(
        AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
        [expected_icon](views::Button* button) {
          std::vector<views::ImageView*> image_views;
          CollectImageViews(button, image_views);
          if (image_views.size() != 1) {
            return false;
          }
          ui::ImageModel image_model = image_views[0]->GetImageModel();
          return image_model.IsVectorIcon() &&
                 image_model.GetVectorIcon().vector_icon() == expected_icon;
        });
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
  std::unique_ptr<views::Widget> bubble_widget_;
  raw_ptr<AnchoredMessageBubbleView> bubble_view_ = nullptr;
};

TEST_F(AnchoredMessageBubbleViewTest, VisibilityReflectsModelOnCreation) {
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kClose));

  bubble_view_->UpdateContent(model_);

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

  bubble_view_->UpdateContent(model_);

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

  bubble_view_->UpdateContent(model_);

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
}

TEST_F(AnchoredMessageBubbleViewTest,
       UpdateContentChangesVisibility_AllVisible_NullMenuModel) {
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kMenu));

  bubble_view_->UpdateContent(model_);

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
}

TEST_F(AnchoredMessageBubbleViewTest, UpdateContentChangesVisibility_ChipOnly) {
  ON_CALL(model_, GetText()).WillByDefault(ReturnRef(empty_text_));
  ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));

  bubble_view_->UpdateContent(model_);

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
}

TEST_F(AnchoredMessageBubbleViewTest, CloseButtonIsFocusable) {
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kClose));

  bubble_view_->UpdateContent(model_);

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId,
                [](views::View* button) { return button->IsFocusable(); }));
}

TEST_F(AnchoredMessageBubbleViewTest, MenuButtonIsFocusable) {
  ui::SimpleMenuModel menu_model(nullptr);
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageActionIconType())
      .WillByDefault(Return(AnchoredMessageActionIconType::kMenu));
  ON_CALL(model_, GetAnchoredMessageMenuModel())
      .WillByDefault(Return(&menu_model));

  bubble_view_->UpdateContent(model_);

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId,
                [](views::View* button) { return button->IsFocusable(); }));
}

TEST_F(AnchoredMessageBubbleViewTest, ExpandButtonFocusRing) {
  std::optional<AnchoredMessageExpandableContent> expandable_content =
      std::make_optional<AnchoredMessageExpandableContent>();
  expandable_content->items.push_back({test_image_, test_text_});

  ON_CALL(model_, GetAnchoredMessageExpandableContent())
      .WillByDefault(ReturnRef(expandable_content));

  bubble_view_->UpdateContent(model_);

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
}

TEST_F(AnchoredMessageBubbleViewTest, ExpandButtonChevron) {
  std::optional<AnchoredMessageExpandableContent> expandable_content =
      std::make_optional<AnchoredMessageExpandableContent>();
  expandable_content->items.push_back({test_image_, test_text_});
  expandable_content->expand_button_style = ExpandButtonStyle::kChevron;

  ON_CALL(model_, GetAnchoredMessageExpandableContent())
      .WillByDefault(ReturnRef(expandable_content));

  bubble_view_->UpdateContent(model_);

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonVectorIcon(&vector_icons::kKeyboardArrowDownIcon),
      // Press to expand
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonVectorIcon(&vector_icons::kKeyboardArrowUpIcon),
      // Press to collapse
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonVectorIcon(&vector_icons::kKeyboardArrowDownIcon));
}

TEST_F(AnchoredMessageBubbleViewTest, ExpandButtonItemIcons) {
  std::optional<AnchoredMessageExpandableContent> expandable_content =
      std::make_optional<AnchoredMessageExpandableContent>();
  expandable_content->items.push_back({test_image_, test_text_});
  expandable_content->items.push_back({test_image_, u"Second item text"});
  expandable_content->expand_button_style = ExpandButtonStyle::kItemIcons;

  ON_CALL(model_, GetAnchoredMessageExpandableContent())
      .WillByDefault(ReturnRef(expandable_content));

  bubble_view_->UpdateContent(model_);

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [this](views::Button* button) {
                  std::vector<views::ImageView*> image_views;
                  CollectImageViews(button, image_views);
                  if (image_views.size() != 2) {
                    return false;
                  }
                  return image_views[0]->GetImageModel() == test_image_ &&
                         image_views[1]->GetImageModel() == test_image_;
                }),
      // Press to expand
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckView(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId,
                [this](views::Button* button) {
                  std::vector<views::ImageView*> image_views;
                  CollectImageViews(button, image_views);
                  if (image_views.size() != 2) {
                    return false;
                  }
                  return image_views[0]->GetImageModel() == test_image_ &&
                         image_views[1]->GetImageModel() == test_image_;
                }));
}

TEST_F(AnchoredMessageBubbleViewTest, ExpandButtonTooltip) {
  std::optional<AnchoredMessageExpandableContent> expandable_content =
      std::make_optional<AnchoredMessageExpandableContent>();
  expandable_content->items.push_back({test_image_, test_text_});

  ON_CALL(model_, GetAnchoredMessageExpandableContent())
      .WillByDefault(ReturnRef(expandable_content));

  bubble_view_->UpdateContent(model_);

  const std::u16string default_expand_tooltip =
      l10n_util::GetStringUTF16(IDS_ANCHORED_MESSAGE_EXPAND_BUTTON_TOOLTIP);
  const std::u16string default_collapse_tooltip =
      l10n_util::GetStringUTF16(IDS_ANCHORED_MESSAGE_COLLAPSE_BUTTON_TOOLTIP);
  const std::u16string default_accessible_name = default_expand_tooltip;

  const std::u16string custom_expand_tooltip = u"Custom expand tooltip";
  const std::u16string custom_collapse_tooltip = u"Custom collapse tooltip";
  const std::u16string custom_accessible_name = u"Custom accessible name";

  RunTestSequence(
      EnsurePresent(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),

      // Default tooltips & stable name (initially collapsed)
      // Name ("Show details") == Tooltip ("Show details") -> Description is
      // empty
      CheckButtonTooltip(default_expand_tooltip),
      CheckAccessibility(default_accessible_name, "", /*expanded=*/false),

      // Expand
      // Name ("Hide details") == Tooltip ("Hide details") -> Description is
      // empty
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonTooltip(default_collapse_tooltip),
      CheckAccessibility(default_collapse_tooltip, "", /*expanded=*/true),

      // Update with custom tooltips only (while expanded).
      // Since accessible_name is nullopt, it defaults to the new expand tooltip
      // ("Custom expand tooltip").
      WithView(AnchoredMessageBubbleView::kAnchoredMessageBubbleId,
               [this, &expandable_content, &custom_expand_tooltip,
                &custom_collapse_tooltip](views::View* view) {
                 auto* bubble_view =
                     static_cast<AnchoredMessageBubbleView*>(view);
                 expandable_content->expand_button_tooltip =
                     custom_expand_tooltip;
                 expandable_content->collapse_button_tooltip =
                     custom_collapse_tooltip;
                 expandable_content->expand_button_accessible_name =
                     std::nullopt;
                 bubble_view->UpdateContent(model_);
               }),

      // Collapse and verify custom expand tooltip
      // Name ("Custom expand tooltip") == Tooltip ("Custom expand tooltip") ->
      // Description is empty
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonTooltip(custom_expand_tooltip),
      CheckAccessibility(custom_expand_tooltip, "", /*expanded=*/false),

      // Expand and verify custom collapse tooltip
      // Name ("Custom collapse tooltip") == Tooltip ("Custom collapse tooltip")
      // -> Description is empty
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonTooltip(custom_collapse_tooltip),
      CheckAccessibility(custom_collapse_tooltip, "", /*expanded=*/true),

      // Update with both custom tooltips and custom accessible name (while
      // expanded)
      // Name becomes "Custom accessible name"
      WithView(AnchoredMessageBubbleView::kAnchoredMessageBubbleId,
               [this, &expandable_content, &custom_expand_tooltip,
                &custom_collapse_tooltip,
                &custom_accessible_name](views::View* view) {
                 auto* bubble_view =
                     static_cast<AnchoredMessageBubbleView*>(view);
                 expandable_content->expand_button_tooltip =
                     custom_expand_tooltip;
                 expandable_content->collapse_button_tooltip =
                     custom_collapse_tooltip;
                 expandable_content->expand_button_accessible_name =
                     custom_accessible_name;
                 bubble_view->UpdateContent(model_);
               }),

      // Collapse and verify custom expand tooltip & custom accessible name
      // Name ("Custom accessible name") != Tooltip ("Custom expand tooltip") ->
      // Description is "Custom expand tooltip"
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonTooltip(custom_expand_tooltip),
      CheckAccessibility(custom_accessible_name,
                         base::UTF16ToUTF8(custom_expand_tooltip),
                         /*expanded=*/false),

      // Expand and verify custom collapse tooltip & custom accessible name
      // Name ("Custom accessible name") != Tooltip ("Custom collapse tooltip")
      // -> Description is "Custom collapse tooltip"
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      CheckButtonTooltip(custom_collapse_tooltip),
      CheckAccessibility(custom_accessible_name,
                         base::UTF16ToUTF8(custom_collapse_tooltip),
                         /*expanded=*/true));
}
}  // namespace page_actions
