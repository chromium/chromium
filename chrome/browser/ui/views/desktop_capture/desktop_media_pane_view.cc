// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_pane_view.h"

#include "chrome/browser/ui/views/desktop_capture/desktop_media_permission_pane_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/desktop_capture/desktop_media_permission_pane_view.h"
#endif

DesktopMediaPaneView::DesktopMediaPaneView(
    DesktopMediaList::Type type,
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<ShareAudioView> share_audio_view)
    : type_(type) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(0)));

  // TODO(crbug.com/339311813): Hide content_pane_view_ from the start if
  // lacking permission.
  content_pane_view_ =
      AddChildView(std::make_unique<DesktopMediaContentPaneView>(
          std::move(content_view), std::move(share_audio_view)));
  layout_->SetFlexForView(content_pane_view_, 1);
}

DesktopMediaPaneView::~DesktopMediaPaneView() = default;

bool DesktopMediaPaneView::AudioOffered() const {
  return content_pane_view_->AudioOffered();
}

bool DesktopMediaPaneView::IsAudioSharingApprovedByUser() const {
  return content_pane_view_->IsAudioSharingApprovedByUser();
}

void DesktopMediaPaneView::SetAudioSharingApprovedByUser(bool is_on) {
  content_pane_view_->SetAudioSharingApprovedByUser(is_on);
}

std::u16string DesktopMediaPaneView::GetAudioLabelText() const {
  return content_pane_view_->GetAudioLabelText();
}

void DesktopMediaPaneView::OnScreenCapturePermissionUpdate(
    bool has_permission) {
#if BUILDFLAG(IS_MAC)
  if (!PermissionRequired()) {
    return;
  }

  if (!has_permission && !permission_pane_view_) {
    MakePermissionPaneView();
  }

  // permission_pane_view_ is only constructed when needed and otherwise the
  // visibilities do not need to be changed.
  if (permission_pane_view_) {
    content_pane_view_->SetVisible(has_permission);
    permission_pane_view_->SetVisible(!has_permission);
  }
#endif
}

bool DesktopMediaPaneView::IsPermissionPaneVisible() const {
  return permission_pane_view_ && permission_pane_view_->GetVisible();
}

bool DesktopMediaPaneView::IsContentPaneVisible() const {
  return content_pane_view_->GetVisible();
}

bool DesktopMediaPaneView::WasPermissionButtonClicked() const {
#if BUILDFLAG(IS_MAC)
  return permission_pane_view_ &&
         permission_pane_view_->WasPermissionButtonClicked();
#else
  return false;
#endif
}

bool DesktopMediaPaneView::PermissionRequired() const {
#if BUILDFLAG(IS_MAC)
  switch (type_) {
    case DesktopMediaList::Type::kScreen:
    case DesktopMediaList::Type::kWindow:
      return true;

    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      return false;
  }
  NOTREACHED_NORETURN();
#else
  return false;
#endif
}

void DesktopMediaPaneView::MakePermissionPaneView() {
#if BUILDFLAG(IS_MAC)
  CHECK(!permission_pane_view_);

  permission_pane_view_ =
      AddChildView(std::make_unique<DesktopMediaPermissionPaneView>(type_));
  layout_->SetFlexForView(permission_pane_view_, 1);
#else
  NOTREACHED_NORETURN();
#endif
}

BEGIN_METADATA(DesktopMediaPaneView)
END_METADATA
