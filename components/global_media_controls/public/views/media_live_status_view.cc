// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/views/media_live_status_view.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace global_media_controls {

namespace {

constexpr int kViewHeight = 28;
constexpr int kLineViewHeight = 2;
constexpr int kLiveLabelPadding = 4;
constexpr int kLiveLabelCornerRadius = 4;

}  // namespace

MediaLiveStatusView::MediaLiveStatusView(ui::ColorId foreground_color_id,
                                         ui::ColorId background_color_id) {
  line_view_ = AddChildView(std::make_unique<views::View>());
  line_view_->SetBackground(views::CreateThemedRoundedRectBackground(
      background_color_id, kLineViewHeight / 2));

  live_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_MEDIA_LIVE_TEXT),
      views::style::CONTEXT_LABEL, views::style::STYLE_CAPTION_BOLD));
  live_label_->SetAutoColorReadabilityEnabled(false);
  live_label_->SetEnabledColorId(foreground_color_id);
  live_label_->SetBorder(views::CreateEmptyBorder(kLiveLabelPadding));
  live_label_->SetBackground(views::CreateThemedRoundedRectBackground(
      background_color_id, kLiveLabelCornerRadius));
  live_label_->SetPaintToLayer();
  live_label_->layer()->SetFillsBoundsOpaquely(false);
}

MediaLiveStatusView::~MediaLiveStatusView() = default;

gfx::Size MediaLiveStatusView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(GetContentsBounds().size().width(), kViewHeight);
}

void MediaLiveStatusView::Layout(PassKey) {
  const int kViewWidth = GetContentsBounds().size().width();
  line_view_->SetBounds(0, (kViewHeight - kLineViewHeight) / 2, kViewWidth,
                        kLineViewHeight);

  const gfx::Size kLiveLabelSize = live_label_->GetPreferredSize();
  live_label_->SetBounds((kViewWidth - kLiveLabelSize.width()) / 2,
                         (kViewHeight - kLiveLabelSize.height()) / 2,
                         kLiveLabelSize.width(), kLiveLabelSize.height());
}

views::View* MediaLiveStatusView::GetLineViewForTesting() {
  return line_view_;
}

views::Label* MediaLiveStatusView::GetLiveLabelForTesting() {
  return live_label_;
}

BEGIN_METADATA(MediaLiveStatusView)
END_METADATA

}  // namespace global_media_controls
