// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_content_pane_view.h"

#include <memory>
#include <string_view>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

DesktopMediaContentPaneView::DesktopMediaContentPaneView(
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<ShareAudioView> share_audio_view) {
  float bottom_radius = 8;
  SetBackground(views::CreateRoundedRectBackground(ui::kColorSysSurface4,
                                                   /*top_radius=*/0,
                                                   bottom_radius));
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

#if BUILDFLAG(IS_MAC)
  audio_warning_view_ =
      AddChildView(std::make_unique<AudioPermissionWarningView>(
          base::BindRepeating(&DesktopMediaContentPaneView::CancelAudioSharing,
                              base::Unretained(this))));
  audio_warning_view_->SetVisible(false);
#endif  // BUILDFLAG(IS_MAC)

  share_audio_view_ = AddChildView(std::move(share_audio_view));
}

DesktopMediaContentPaneView::~DesktopMediaContentPaneView() = default;

bool DesktopMediaContentPaneView::AudioOffered() const {
  return share_audio_view_ && share_audio_view_->AudioOffered();
}

bool DesktopMediaContentPaneView::IsAudioSharingApprovedByUser() const {
  return share_audio_view_ && share_audio_view_->IsAudioSharingApprovedByUser();
}

void DesktopMediaContentPaneView::SetAudioSharingApprovedByUser(bool is_on) {
  CHECK(share_audio_view_);
  share_audio_view_->SetAudioSharingApprovedByUser(is_on);
}

bool DesktopMediaContentPaneView::IsAudioSharingControlEnabled() const {
  CHECK(share_audio_view_);
  return share_audio_view_->GetEnabled();
}

void DesktopMediaContentPaneView::SetAudioSharingControlEnabled(bool enabled) {
  CHECK(share_audio_view_);
  share_audio_view_->SetEnabled(enabled);
}

std::u16string_view DesktopMediaContentPaneView::GetAudioLabelText() const {
  return share_audio_view_ ? share_audio_view_->GetAudioLabelText()
                           : std::u16string_view();
}

#if BUILDFLAG(IS_MAC)
void DesktopMediaContentPaneView::SetAudioWarningVisible(bool visible) {
  if (audio_warning_view_) {
    audio_warning_view_->SetWarningVisible(visible);
    if (!visible) {
      share_audio_view_->RequestFocus();
    }
  }
}

bool DesktopMediaContentPaneView::IsAudioWarningVisible() const {
  return audio_warning_view_ && audio_warning_view_->GetVisible();
}

void DesktopMediaContentPaneView::CancelAudioSharing() {
  SetAudioSharingApprovedByUser(false);
  SetAudioWarningVisible(false);
}
#endif  // BUILDFLAG(IS_MAC)

BEGIN_METADATA(DesktopMediaContentPaneView)
END_METADATA
