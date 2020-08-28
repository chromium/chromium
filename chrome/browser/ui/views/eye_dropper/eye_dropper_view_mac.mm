// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view_mac.h"

#import <Cocoa/Cocoa.h>

#include "content/public/browser/render_frame_host.h"
#include "skia/ext/skia_utils_mac.h"

EyeDropperViewMac::EyeDropperViewMac(content::EyeDropperListener* listener)
    : listener_(listener) {
  if (!listener_)
    return;
  if (@available(macOS 10.15, *)) {
    color_sampler_.reset([[NSColorSampler alloc] init]);
    [color_sampler_ showSamplerWithSelectionHandler:^(NSColor* selectedColor) {
      if (!selectedColor) {
        listener_->ColorSelectionCanceled();
      } else {
        listener_->ColorSelected(skia::NSSystemColorToSkColor(selectedColor));
      }
    }];
  }
}

EyeDropperViewMac::~EyeDropperViewMac() {}

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  if (@available(macOS 10.15, *)) {
    return std::make_unique<EyeDropperViewMac>(listener);
  }
  return nullptr;
}
