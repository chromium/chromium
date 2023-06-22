// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_pane_view.h"

#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

DesktopMediaPaneView::DesktopMediaPaneView(
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<ShareAudioView> share_audio_view) {
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorSubtleEmphasisBackground));
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(0, 16)));
  layout->SetFlexForView(AddChildView(std::move(content_view)), 1);

  if (!share_audio_view) {
    return;
  }

  AddChildView(std::make_unique<views::Separator>());
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
