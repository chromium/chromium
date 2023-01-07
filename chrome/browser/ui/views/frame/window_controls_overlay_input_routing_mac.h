// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOW_CONTROLS_OVERLAY_INPUT_ROUTING_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOW_CONTROLS_OVERLAY_INPUT_ROUTING_MAC_H_

#include "base/memory/raw_ptr.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
class NativeWidgetMacNSWindowHost;
}  // namespace views

class BrowserNonClientFrameViewMac;

// WindowControlsOverlayInputRoutingMac is responsible for adding a remote
// NSView and positioning it correctly based on the position of the provided
// |overlay_view|. Intended for PWAs with window controls overlay display
// override.
class WindowControlsOverlayInputRoutingMac : public views::ViewObserver {
 public:
  WindowControlsOverlayInputRoutingMac(
      BrowserNonClientFrameViewMac* browser_non_client_frame_view_mac,
      views::View* overlay_view,
      remote_cocoa::mojom::WindowControlsOverlayNSViewType overlay_type);
  ~WindowControlsOverlayInputRoutingMac() override;

  void Enable();
  void Disable();

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* overlay_view) override;

 private:
  void UpdateNSViewPosition();

  raw_ptr<BrowserNonClientFrameViewMac> browser_non_client_frame_view_mac_ =
      nullptr;  // weak
  raw_ptr<views::View>
      overlay_view_;  // weak. Owned by BrowserNonClientFrameViewMac.
  raw_ptr<views::NativeWidgetMacNSWindowHost>
      host_;  // weak. Owned by NativeWidgetMac.
  remote_cocoa::mojom::WindowControlsOverlayNSViewType overlay_type_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOW_CONTROLS_OVERLAY_INPUT_ROUTING_MAC_H_
