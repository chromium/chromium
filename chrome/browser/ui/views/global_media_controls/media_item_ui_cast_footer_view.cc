// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_cast_footer_view.h"

#include "components/media_message_center/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr int kButtonHeight = 32;
constexpr int kIconSize = 20;
constexpr int kImageLabelSpacing = 5;

constexpr auto kViewInsets = gfx::Insets::VH(0, 5);
constexpr auto kButtonInsets = gfx::Insets::VH(5, 8);

}  // namespace

MediaItemUICastFooterView::MediaItemUICastFooterView(
    base::RepeatingClosure stop_casting_callback,
    media_message_center::MediaColorTheme media_color_theme)
    : stop_casting_callback_(std::move(stop_casting_callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kViewInsets));

  stop_casting_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&MediaItemUICastFooterView::StopCasting,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_STOP_CASTING)));
  stop_casting_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_STOP_CASTING));
  stop_casting_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          media_message_center::kMediaCastStopIcon,
          media_color_theme.error_foreground_color_id, kIconSize));

  stop_casting_button_->SetEnabledTextColorIds(
      media_color_theme.error_foreground_color_id);
  stop_casting_button_->SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);
  stop_casting_button_->SetImageLabelSpacing(kImageLabelSpacing);
  stop_casting_button_->SetBorder(views::CreateEmptyBorder(kButtonInsets));

  stop_casting_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      media_color_theme.error_container_color_id, kButtonHeight / 2));
  stop_casting_button_->SetFocusRingCornerRadius(kButtonHeight / 2);
  views::FocusRing::Get(stop_casting_button_)
      ->SetColorId(media_color_theme.focus_ring_color_id);

  layout->SetFlexForView(stop_casting_button_, 1);
}

MediaItemUICastFooterView::~MediaItemUICastFooterView() = default;

views::Button* MediaItemUICastFooterView::GetStopCastingButtonForTesting() {
  return stop_casting_button_;
}

void MediaItemUICastFooterView::StopCasting() {
  stop_casting_button_->SetEnabled(false);
  stop_casting_callback_.Run();
}
