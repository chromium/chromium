// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_NATIVE_WIDGET_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_NATIVE_WIDGET_MAC_H_

#include "ui/views/widget/native_widget_mac.h"

class VideoOverlayWindowViews;

// This NativeWidget allows VideoOverlayWindowViews to have default Mac window
// styling despite removing the standard frame.
class VideoOverlayWindowNativeWidgetMac : public views::NativeWidgetMac {
 public:
  VideoOverlayWindowNativeWidgetMac(VideoOverlayWindowViews* widget);
  VideoOverlayWindowNativeWidgetMac(const VideoOverlayWindowNativeWidgetMac&) =
      delete;
  VideoOverlayWindowNativeWidgetMac& operator=(
      const VideoOverlayWindowNativeWidgetMac&) = delete;

  ~VideoOverlayWindowNativeWidgetMac() override;

 protected:
  // NativeWidgetMac:
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_NATIVE_WIDGET_MAC_H_
