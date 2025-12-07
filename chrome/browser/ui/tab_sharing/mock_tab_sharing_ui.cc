// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/mock_tab_sharing_ui.h"

MockTabSharingUI::MockTabSharingUI()
    : uma_logger_(content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {}

MockTabSharingUI::~MockTabSharingUI() = default;

gfx::NativeViewId MockTabSharingUI::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback,
    const std::vector<content::DesktopMediaID>& media_ids) {
  return 0;
}

ScreensharingControlsHistogramLogger& MockTabSharingUI::GetUmaLogger() {
  return uma_logger_;
}

void MockTabSharingUI::OnRegionCaptureRectChanged(
    const std::optional<gfx::Rect>& region_capture_rect) {}
