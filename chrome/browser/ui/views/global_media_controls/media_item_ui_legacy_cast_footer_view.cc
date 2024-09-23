// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_legacy_cast_footer_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr auto kInsets = gfx::Insets::VH(6, 10);
constexpr gfx::Size kSize{400, 40};
constexpr auto kBorderInsets = gfx::Insets::VH(4, 8);

}  // anonymous namespace

MediaItemUILegacyCastFooterView::MediaItemUILegacyCastFooterView(
    base::RepeatingClosure stop_casting_callback)
    : stop_casting_callback_(stop_casting_callback) {
  DCHECK(!stop_casting_callback_.is_null());

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kInsets));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetPreferredSize(kSize);

  stop_cast_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&MediaItemUILegacyCastFooterView::StopCasting,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_STOP_CASTING_BUTTON_LABEL)));
  const int radius = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, kSize);
  views::InstallRoundRectHighlightPathGenerator(stop_cast_button_,
                                                gfx::Insets(), radius);

  views::InkDrop::Get(stop_cast_button_)
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  stop_cast_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  stop_cast_button_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(1, radius, foreground_color_),
      kBorderInsets));
  UpdateColors();
}

MediaItemUILegacyCastFooterView::~MediaItemUILegacyCastFooterView() = default;

views::Button*
MediaItemUILegacyCastFooterView::GetStopCastingButtonForTesting() {
  return stop_cast_button_;
}

void MediaItemUILegacyCastFooterView::OnColorsChanged(SkColor foreground,
                                                      SkColor background) {
  if (foreground == foreground_color_ && background == background_color_)
    return;

  foreground_color_ = foreground;
  background_color_ = background;

  UpdateColors();
}

void MediaItemUILegacyCastFooterView::StopCasting() {
  stop_cast_button_->SetEnabled(false);
  stop_casting_callback_.Run();
}

void MediaItemUILegacyCastFooterView::UpdateColors() {
  // Update background.
  SetBackground(views::CreateSolidBackground(background_color_));

  // Update button icon.
  stop_cast_button_->SetEnabledTextColors(foreground_color_);
  views::InkDrop::Get(stop_cast_button_)->SetBaseColor(foreground_color_);
  const int radius = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, kSize);
  stop_cast_button_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(1, radius, foreground_color_),
      kBorderInsets));
}

BEGIN_METADATA(MediaItemUILegacyCastFooterView)
END_METADATA
