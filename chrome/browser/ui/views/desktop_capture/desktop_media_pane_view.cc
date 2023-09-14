// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_pane_view.h"

#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

DesktopMediaPaneView::DesktopMediaPaneView(
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<ShareAudioView> share_audio_view) {
  float bottom_radius = features::IsChromeRefresh2023() ? 8 : 4;
  SetBackground(views::CreateThemedRoundedRectBackground(
      features::IsChromeRefresh2023() ? ui::kColorSysSurface4
                                      : ui::kColorSubtleEmphasisBackground,
      /*top_radius=*/0, bottom_radius,
      /*for_border_thickness=*/0));
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(0)));
  layout->SetFlexForView(AddChildView(std::move(content_view)), 1);

  if (!share_audio_view) {
    SetPaintToLayer();
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(0, 0, bottom_radius, bottom_radius));
    return;
  }

  View* separator_container = AddChildView(std::make_unique<views::View>());
  separator_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(0, 16)));
  separator_container->AddChildView(std::make_unique<views::Separator>());
  share_audio_view_ = AddChildView(std::move(share_audio_view));
}

DesktopMediaPaneView::~DesktopMediaPaneView() = default;

bool DesktopMediaPaneView::AudioOffered() const {
  return share_audio_view_ && share_audio_view_->AudioOffered();
}

bool DesktopMediaPaneView::IsAudioSharingApprovedByUser() const {
  return share_audio_view_ && share_audio_view_->IsAudioSharingApprovedByUser();
}

void DesktopMediaPaneView::SetAudioSharingApprovedByUser(bool is_on) {
  CHECK(share_audio_view_);
  share_audio_view_->SetAudioSharingApprovedByUser(is_on);
}
