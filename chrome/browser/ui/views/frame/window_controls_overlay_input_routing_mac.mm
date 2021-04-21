// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/frame/window_controls_overlay_input_routing_mac.h"

#include <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/widget.h"

WindowControlsOverlayInputRoutingMac::WindowControlsOverlayInputRoutingMac(
    BrowserNonClientFrameViewMac const* browser_non_client_frame_view_mac,
    views::View* overlay_view,
    remote_cocoa::mojom::WindowControlsOverlayNSViewType overlay_type)
    : browser_non_client_frame_view_mac(browser_non_client_frame_view_mac),
      overlay_view_(overlay_view),
      overlay_type_(overlay_type) {
  overlay_view_->AddObserver(this);
}

WindowControlsOverlayInputRoutingMac::~WindowControlsOverlayInputRoutingMac() {
  overlay_view_->RemoveObserver(this);
}

void WindowControlsOverlayInputRoutingMac::OnViewBoundsChanged(
    views::View* overlay_view) {
  UpdateNSViewPosition();
}

void WindowControlsOverlayInputRoutingMac::Enable() {
  auto window =
      browser_non_client_frame_view_mac->GetWidget()->GetNativeWindow();
  host_ = views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(window);

  host_->AddRemoteWindowControlsOverlayView(overlay_type_);

  UpdateNSViewPosition();
}

void WindowControlsOverlayInputRoutingMac::Disable() {
  if (!host_)
    return;
  // TODO(crbug.com/937121): Implement when toggle button is added.
}

void WindowControlsOverlayInputRoutingMac::UpdateNSViewPosition() {
  if (!host_)
    return;

  host_->UpdateRemoteWindowControlsOverlayView(
      overlay_view_->GetMirroredBounds(), overlay_type_);
}
