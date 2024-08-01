// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_action_button.h"

#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/test/views_test_base.h"

namespace global_media_controls {

namespace {

using media_session::mojom::MediaSessionAction;

constexpr int kTooltipTextId =
    IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE;

constexpr int kUpdatedTooltipTextId =
    IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY;

constexpr int kIconSize = 10;

constexpr ui::ColorId kIconColorId = ui::kColorSysSurface;

std::unique_ptr<MediaActionButton> CreateMediaActionButton(int button_id) {
  return std::make_unique<MediaActionButton>(
      views::Button::PressedCallback(), button_id, kTooltipTextId, kIconSize,
      vector_icons::kPauseIcon, /*button_size=*/gfx::Size(20, 20), kIconColorId,
      ui::kColorSysSurface1, ui::kColorSysSurface2);
}

}  // namespace

using MediaActionButtonTest = views::ViewsTestBase;

TEST_F(MediaActionButtonTest, CheckFlipCanvasOnPaintForRTLUI) {
  auto pause_button =
      CreateMediaActionButton(static_cast<int>(MediaSessionAction::kPause));
  EXPECT_FALSE(pause_button->GetFlipCanvasOnPaintForRTLUI());

  auto previous_track_button = CreateMediaActionButton(
      static_cast<int>(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(previous_track_button->GetFlipCanvasOnPaintForRTLUI());

  auto next_track_button =
      CreateMediaActionButton(static_cast<int>(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(next_track_button->GetFlipCanvasOnPaintForRTLUI());
}

TEST_F(MediaActionButtonTest, UpdateButton) {
  auto button =
      CreateMediaActionButton(static_cast<int>(MediaSessionAction::kPause));
  EXPECT_EQ(button->GetID(), static_cast<int>(MediaSessionAction::kPause));
  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(kTooltipTextId));

  button->Update(static_cast<int>(MediaSessionAction::kPlay),
                 vector_icons::kPlayArrowIcon, kUpdatedTooltipTextId,
                 kIconColorId);
  EXPECT_EQ(button->GetID(), static_cast<int>(MediaSessionAction::kPlay));
  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(kUpdatedTooltipTextId));
}

TEST_F(MediaActionButtonTest, UpdateButtonText) {
  auto button =
      CreateMediaActionButton(static_cast<int>(MediaSessionAction::kPause));
  button->UpdateText(kUpdatedTooltipTextId);
  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(kUpdatedTooltipTextId));
}

TEST_F(MediaActionButtonTest, UpdateButtonIcon) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* button = widget->SetContentsView(
      CreateMediaActionButton(static_cast<int>(MediaSessionAction::kPause)));
  widget->Show();

  button->UpdateIcon(vector_icons::kCastIcon);
  SkBitmap expected =
      *gfx::CreateVectorIcon(vector_icons::kCastIcon, kIconSize,
                             widget->GetColorProvider()->GetColor(kIconColorId))
           .bitmap();
  SkBitmap actual = *button->GetImage(views::Button::STATE_NORMAL).bitmap();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(expected, actual));
}

}  // namespace global_media_controls
