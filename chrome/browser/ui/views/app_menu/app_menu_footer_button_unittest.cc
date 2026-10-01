// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_footer_button.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/test_menu_item_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class AppMenuFooterButtonTest : public ChromeViewsTestBase {
 public:
  AppMenuFooterButtonTest() = default;
  ~AppMenuFooterButtonTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    root_menu_item_ = std::make_unique<views::TestMenuItemView>();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    root_menu_item_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  // Creates a pill-style footer button with an icon.
  void CreatePillButton() {
    button_ = widget_->SetContentsView(std::make_unique<AppMenuFooterButton>());
    button_->SetText(u"Settings");
    button_->SetImageModel(ui::ImageModel::FromVectorIcon(kCircleFilledIcon));
    button_->SetUseRowStyle(false);
  }

  // Creates a row-style footer button, like the managed-browser footer item.
  void CreateRowButton() {
    button_ = widget_->SetContentsView(std::make_unique<AppMenuFooterButton>());
    button_->SetText(u"Managed by your organization");
    button_->SetUseRowStyle(true);
  }

  // Creates a pill-style footer button that opens a submenu, like Help.
  views::MenuItemView* CreateSubmenuButton() {
    views::MenuItemView* submenu_item =
        root_menu_item_->AppendSubMenu(1, u"Help");
    button_ = widget_->SetContentsView(
        std::make_unique<AppMenuFooterButton>(submenu_item));
    button_->SetText(u"Help");
    button_->SetUseRowStyle(false);
    return submenu_item;
  }

  ui::ColorVariant GetLabelColor() {
    for (views::View* child : button_->children()) {
      if (auto* label = views::AsViewClass<views::Label>(child)) {
        return label->GetRequestedEnabledColor().value();
      }
    }
    NOTREACHED();
  }

  // Returns the button's `views::ImageView` children in order: the icon first
  // and, for submenu buttons, the submenu arrow last. Children are looked up by
  // type because `views::Button` also adds a `views::FocusRing` child.
  std::vector<views::ImageView*> GetImageViews() {
    std::vector<views::ImageView*> image_views;
    for (views::View* child : button_->children()) {
      if (auto* image_view = views::AsViewClass<views::ImageView>(child)) {
        image_views.push_back(image_view);
      }
    }
    return image_views;
  }

  ui::ColorVariant GetIconColor() {
    std::vector<views::ImageView*> image_views = GetImageViews();
    CHECK(!image_views.empty());
    return image_views.front()->GetImageModel().GetVectorIcon().color();
  }

  ui::ColorVariant GetSubmenuArrowColor() {
    std::vector<views::ImageView*> image_views = GetImageViews();
    CHECK(!image_views.empty());
    return image_views.back()->GetImageModel().GetVectorIcon().color();
  }

  views::InkDropHost::InkDropMode GetInkDropMode() {
    return views::InkDrop::Get(button_)->GetMode();
  }

  std::unique_ptr<views::TestMenuItemView> root_menu_item_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AppMenuFooterButton> button_ = nullptr;
};

TEST_F(AppMenuFooterButtonTest, PillLabelUsesDisabledColorWhenDisabled) {
  CreatePillButton();

  button_->SetEnabled(false);

  EXPECT_EQ(GetLabelColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorButtonForegroundDisabled}));
}

TEST_F(AppMenuFooterButtonTest, PillLabelRestoresEnabledColorWhenReEnabled) {
  CreatePillButton();

  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetLabelColor(),
            ui::ColorVariant(ui::ColorId{kColorAppMenuFooterButtonForeground}));
}

TEST_F(AppMenuFooterButtonTest, RowLabelUsesDisabledColorWhenDisabled) {
  CreateRowButton();

  button_->SetEnabled(false);

  EXPECT_EQ(
      GetLabelColor(),
      ui::ColorVariant(ui::ColorId{ui::kColorMenuItemForegroundDisabled}));
}

TEST_F(AppMenuFooterButtonTest, RowLabelRestoresEnabledColorWhenReEnabled) {
  CreateRowButton();

  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetLabelColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorMenuItemForeground}));
}

TEST_F(AppMenuFooterButtonTest, IconUsesDisabledColorWhenDisabled) {
  CreatePillButton();

  button_->SetEnabled(false);

  EXPECT_EQ(GetIconColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorMenuIconDisabled}));
}

TEST_F(AppMenuFooterButtonTest, IconRestoresEnabledColorWhenReEnabled) {
  CreatePillButton();

  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetIconColor(),
            ui::ColorVariant(ui::ColorId{kColorAppMenuFooterButtonForeground}));
}

TEST_F(AppMenuFooterButtonTest, IconSetWhileDisabledUsesDisabledColor) {
  CreatePillButton();
  button_->SetEnabled(false);

  button_->SetImageModel(ui::ImageModel::FromVectorIcon(kCircleFilledIcon));

  EXPECT_EQ(GetIconColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorMenuIconDisabled}));
}

TEST_F(AppMenuFooterButtonTest, SubmenuArrowUsesDisabledColorWhenDisabled) {
  CreateSubmenuButton();

  button_->SetEnabled(false);

  EXPECT_EQ(GetSubmenuArrowColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorMenuIconDisabled}));
}

TEST_F(AppMenuFooterButtonTest, SubmenuArrowRestoresEnabledColorWhenReEnabled) {
  CreateSubmenuButton();

  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetSubmenuArrowColor(),
            ui::ColorVariant(ui::ColorId{kColorAppMenuFooterButtonForeground}));
}

TEST_F(AppMenuFooterButtonTest, InkDropIsOffWhenDisabled) {
  CreatePillButton();

  button_->SetEnabled(false);

  EXPECT_EQ(GetInkDropMode(), views::InkDropHost::InkDropMode::OFF);
}

TEST_F(AppMenuFooterButtonTest, InkDropIsOnWhenReEnabled) {
  CreatePillButton();

  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetInkDropMode(), views::InkDropHost::InkDropMode::ON);
}

TEST_F(AppMenuFooterButtonTest, HoverDoesNotShowHighlightWhenDisabled) {
  CreatePillButton();
  button_->SetEnabled(false);

  // Mirrors what `InkDropEventHandler` does on `ui::EventType::kMouseEntered`.
  views::InkDropHost* const ink_drop = views::InkDrop::Get(button_);
  ink_drop->GetInkDrop()->SetHovered(true);

  EXPECT_FALSE(ink_drop->GetInkDrop()->IsHighlightFadingInOrVisible());
}

TEST_F(AppMenuFooterButtonTest, DisabledButtonIsNotFocusable) {
  CreatePillButton();

  button_->SetEnabled(false);

  EXPECT_FALSE(button_->IsFocusable());
}

TEST_F(AppMenuFooterButtonTest, DisabledButtonIsDetachedFromSubmenu) {
  CreateSubmenuButton();

  button_->SetEnabled(false);

  EXPECT_EQ(button_->GetProperty(views::kSubmenuItemKey), nullptr);
}

TEST_F(AppMenuFooterButtonTest, ReEnabledButtonIsReattachedToSubmenu) {
  views::MenuItemView* const submenu_item = CreateSubmenuButton();

  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(button_->GetProperty(views::kSubmenuItemKey), submenu_item);
}

TEST_F(AppMenuFooterButtonTest, DisablingButtonDisablesSubmenuItem) {
  views::MenuItemView* const submenu_item = CreateSubmenuButton();

  button_->SetEnabled(false);

  EXPECT_FALSE(submenu_item->GetEnabled());
}

TEST_F(AppMenuFooterButtonTest, ReEnablingButtonReEnablesSubmenuItem) {
  views::MenuItemView* const submenu_item = CreateSubmenuButton();

  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_TRUE(submenu_item->GetEnabled());
}

}  // namespace
