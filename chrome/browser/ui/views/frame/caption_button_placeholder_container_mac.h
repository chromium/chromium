// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CAPTION_BUTTON_PLACEHOLDER_CONTAINER_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CAPTION_BUTTON_PLACEHOLDER_CONTAINER_MAC_H_

#include "ui/views/view.h"

class BrowserNonClientFrameViewMac;

// A placeholder container for the macOS's traffic lights for PWAs with window
// controls overlay display override. Does not interact with the buttons. It is
// just used to indicate that this is non-client-area.
class CaptionButtonPlaceholderContainerMac : public views::View {
 public:
  explicit CaptionButtonPlaceholderContainerMac(
      BrowserNonClientFrameViewMac* frame_view);

  ~CaptionButtonPlaceholderContainerMac() override;

  CaptionButtonPlaceholderContainerMac(
      const CaptionButtonPlaceholderContainerMac&) = delete;
  CaptionButtonPlaceholderContainerMac& operator=(
      const CaptionButtonPlaceholderContainerMac&) = delete;

  void LayoutForWindowControlsOverlay(const gfx::Rect& bounds);

  // views::View:
  void AddedToWidget() override;

 private:
  BrowserNonClientFrameViewMac* const frame_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CAPTION_BUTTON_PLACEHOLDER_CONTAINER_MAC_H_
