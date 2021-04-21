// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/caption_button_placeholder_container_mac.h"

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

CaptionButtonPlaceholderContainerMac::CaptionButtonPlaceholderContainerMac(
    BrowserNonClientFrameViewMac* frame_view)
    : frame_view_(frame_view) {}

CaptionButtonPlaceholderContainerMac::~CaptionButtonPlaceholderContainerMac() =
    default;

void CaptionButtonPlaceholderContainerMac::LayoutForWindowControlsOverlay(
    const gfx::Rect& bounds) {
  SetBoundsRect(bounds);
  Layout();
}

void CaptionButtonPlaceholderContainerMac::AddedToWidget() {
  SetBackground(views::CreateSolidBackground(frame_view_->GetTitlebarColor()));
  // BrowserView paints to a layer, so this must do the same to ensure that it
  // paints on top of the BrowserView.
  SetPaintToLayer();
}