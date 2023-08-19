// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view_mac.h"

#include <Carbon/Carbon.h>  // For keycode names in Carbon's Event.h.
#import <Cocoa/Cocoa.h>

#include <memory>

#include "content/public/browser/render_frame_host.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/ui_base_features.h"

EyeDropperViewMac::EyeDropperViewMac(content::EyeDropperListener* listener)
    : listener_(listener), weak_ptr_factory_(this) {
  if (!listener_) {
    return;
  }
  color_sampler_ = [[NSColorSampler alloc] init];
  // Used to ensure that EyeDropperViewMac is still alive when the handler is
  // called.
  base::WeakPtr<EyeDropperViewMac> weak_this = weak_ptr_factory_.GetWeakPtr();
  [color_sampler_ showSamplerWithSelectionHandler:^(NSColor* selectedColor) {
    if (!weak_this) {
      return;
    }
    if (!selectedColor) {
      listener_->ColorSelectionCanceled();
    } else {
      listener_->ColorSelected(skia::NSSystemColorToSkColor(selectedColor));
    }
  }];
}

EyeDropperViewMac::~EyeDropperViewMac() = default;

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  if (!features::IsEyeDropperEnabled()) {
    return nullptr;
  }

  return std::make_unique<EyeDropperViewMac>(listener);
}
