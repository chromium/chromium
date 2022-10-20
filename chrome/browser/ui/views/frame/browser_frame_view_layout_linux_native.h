// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_NATIVE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_NATIVE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_frame_provider.h"

// A specialization of BrowserFrameViewLayoutLinux that is also able
// to layout frame buttons that were rendered by the native toolkit.
class BrowserFrameViewLayoutLinuxNative : public BrowserFrameViewLayoutLinux {
 public:
  explicit BrowserFrameViewLayoutLinuxNative(
      ui::NavButtonProvider* nav_button_provider,
      ui::WindowFrameProvider* window_frame_provider);

  BrowserFrameViewLayoutLinuxNative(const BrowserFrameViewLayoutLinuxNative&) =
      delete;
  BrowserFrameViewLayoutLinuxNative& operator=(
      const BrowserFrameViewLayoutLinuxNative&) = delete;

  ~BrowserFrameViewLayoutLinuxNative() override;

 protected:
  // OpaqueBrowserFrameViewLayout:
  int CaptionButtonY(views::FrameButton button_id,
                     bool restored) const override;
  gfx::Insets RestoredFrameBorderInsets() const override;
  TopAreaPadding GetTopAreaPadding(bool has_leading_buttons,
                                   bool has_trailing_buttons) const override;
  int GetWindowCaptionSpacing(views::FrameButton button_id,
                              bool leading_spacing,
                              bool is_leading_button) const override;

 private:
  // Converts a FrameButton to a FrameButtonDisplayType, taking into
  // consideration the maximized state of the browser window.
  ui::NavButtonProvider::FrameButtonDisplayType GetButtonDisplayType(
      views::FrameButton button_id) const;

  // Owned by BrowserFrameViewLinuxNative.
  const raw_ptr<ui::NavButtonProvider, DanglingUntriaged> nav_button_provider_;
  const raw_ptr<ui::WindowFrameProvider, DanglingUntriaged>
      window_frame_provider_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_NATIVE_H_
