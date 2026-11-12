// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/mute_button.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

namespace voice {

namespace {

constexpr char16_t kMuteLabel[] = u"Mute";
constexpr char16_t kUnmuteLabel[] = u"Unmute";

testing::AssertionResult ShowsIcon(const MuteButton& button,
                                   const gfx::VectorIcon& icon) {
  const int size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_ICON_SIZE);
  if (button.GetImageModel(views::Button::STATE_NORMAL) !=
      ui::ImageModel::FromVectorIcon(icon, ui::kColorButtonForegroundProminent,
                                     size)) {
    return testing::AssertionFailure() << "wrong normal icon";
  }
  if (button.GetImageModel(views::Button::STATE_DISABLED) !=
      ui::ImageModel::FromVectorIcon(icon, ui::kColorButtonForegroundDisabled,
                                     size)) {
    return testing::AssertionFailure() << "wrong disabled icon";
  }
  return testing::AssertionSuccess();
}

class MuteButtonTest : public ChromeViewsTestBase {
 public:
  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  MuteButton* CreateMuteButton() {
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    MuteButton* const button =
        widget_->SetContentsView(std::make_unique<MuteButton>(
            views::Button::PressedCallback(), kMuteLabel, kUnmuteLabel));
    widget_->Show();
    return button;
  }

 private:
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MuteButtonTest, LabelsFollowMuted) {
  MuteButton* const button = CreateMuteButton();
  EXPECT_EQ(button->GetTooltipText(), kMuteLabel);
  EXPECT_EQ(button->GetAccessibleName(), kMuteLabel);

  button->SetMuted(true);
  EXPECT_EQ(button->GetTooltipText(), kUnmuteLabel);
  EXPECT_EQ(button->GetAccessibleName(), kUnmuteLabel);

  button->SetMuted(false);
  EXPECT_EQ(button->GetTooltipText(), kMuteLabel);
  EXPECT_EQ(button->GetAccessibleName(), kMuteLabel);
}

TEST_F(MuteButtonTest, IconFollowsMuted) {
  const bool rounded = features::IsRoundedIconsEnabled();
  const gfx::VectorIcon& mic_icon =
      rounded ? vector_icons::kMicIcon : vector_icons::kMicChromeRefreshOldIcon;
  const gfx::VectorIcon& mic_off_icon =
      rounded ? vector_icons::kMicOffIcon
              : vector_icons::kMicOffChromeRefreshOldIcon;
  MuteButton* const button = CreateMuteButton();
  EXPECT_TRUE(ShowsIcon(*button, mic_icon));

  button->SetMuted(true);
  EXPECT_TRUE(ShowsIcon(*button, mic_off_icon));

  button->SetMuted(false);
  EXPECT_TRUE(ShowsIcon(*button, mic_icon));
}

}  // namespace

}  // namespace voice
