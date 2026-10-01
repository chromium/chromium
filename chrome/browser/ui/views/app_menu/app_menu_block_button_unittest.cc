// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_block_button.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class AppMenuBlockButtonTest : public ChromeViewsTestBase {
 public:
  AppMenuBlockButtonTest() = default;
  ~AppMenuBlockButtonTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<AppMenuBlockButton>());
    button_->SetText(u"New tab");
    button_->SetImageModel(ui::ImageModel::FromVectorIcon(kCircleFilledIcon));
    widget_->Show();
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  // Returns the first direct child of `button_` of type `T`.
  template <typename T>
  T* GetChildOfType() {
    for (views::View* child : button_->children()) {
      if (auto* typed_child = views::AsViewClass<T>(child)) {
        return typed_child;
      }
    }
    return nullptr;
  }

  ui::ColorVariant GetLabelColor() {
    return GetChildOfType<views::Label>()->GetRequestedEnabledColor().value();
  }

  ui::ColorVariant GetIconColor() {
    return GetChildOfType<views::ImageView>()
        ->GetImageModel()
        .GetVectorIcon()
        .color();
  }

  ui::ColorVariant GetBorderColor() { return button_->GetBorder()->color(); }

  views::InkDropHost::InkDropMode GetInkDropMode() {
    return views::InkDrop::Get(button_)->GetMode();
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AppMenuBlockButton> button_ = nullptr;
};

TEST_F(AppMenuBlockButtonTest, LabelUsesDisabledColorWhenDisabled) {
  button_->SetEnabled(false);

  EXPECT_EQ(GetLabelColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorButtonForegroundDisabled}));
}

TEST_F(AppMenuBlockButtonTest, LabelRestoresEnabledColorWhenReEnabled) {
  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetLabelColor(),
            ui::ColorVariant(ui::ColorId{kColorAppMenuBlockButtonForeground}));
}

TEST_F(AppMenuBlockButtonTest, IconUsesDisabledColorWhenDisabled) {
  button_->SetEnabled(false);

  EXPECT_EQ(GetIconColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorButtonForegroundDisabled}));
}

TEST_F(AppMenuBlockButtonTest, IconRestoresEnabledColorWhenReEnabled) {
  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetIconColor(),
            ui::ColorVariant(ui::ColorId{kColorAppMenuBlockButtonForeground}));
}

TEST_F(AppMenuBlockButtonTest, IconSetWhileDisabledUsesDisabledColor) {
  button_->SetEnabled(false);

  button_->SetImageModel(ui::ImageModel::FromVectorIcon(kCircleFilledIcon));

  EXPECT_EQ(GetIconColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorButtonForegroundDisabled}));
}

TEST_F(AppMenuBlockButtonTest, BorderUsesDisabledColorWhenDisabled) {
  button_->SetEnabled(false);

  EXPECT_EQ(GetBorderColor(),
            ui::ColorVariant(ui::ColorId{ui::kColorButtonBorderDisabled}));
}

TEST_F(AppMenuBlockButtonTest, BorderRestoresEnabledColorWhenReEnabled) {
  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetBorderColor(),
            ui::ColorVariant(ui::ColorId{kColorAppMenuBlockButtonBorder}));
}

TEST_F(AppMenuBlockButtonTest, InkDropIsOffWhenDisabled) {
  button_->SetEnabled(false);

  EXPECT_EQ(GetInkDropMode(), views::InkDropHost::InkDropMode::OFF);
}

TEST_F(AppMenuBlockButtonTest, InkDropIsOnWhenReEnabled) {
  button_->SetEnabled(false);
  button_->SetEnabled(true);

  EXPECT_EQ(GetInkDropMode(), views::InkDropHost::InkDropMode::ON);
}

TEST_F(AppMenuBlockButtonTest, HoverDoesNotShowHighlightWhenDisabled) {
  button_->SetEnabled(false);

  // Mirrors what `InkDropEventHandler` does on `ui::EventType::kMouseEntered`.
  views::InkDropHost* const ink_drop = views::InkDrop::Get(button_);
  ink_drop->GetInkDrop()->SetHovered(true);

  EXPECT_FALSE(ink_drop->GetInkDrop()->IsHighlightFadingInOrVisible());
}

TEST_F(AppMenuBlockButtonTest, DisabledButtonIsNotFocusable) {
  button_->SetEnabled(false);

  EXPECT_FALSE(button_->IsFocusable());
}

}  // namespace
