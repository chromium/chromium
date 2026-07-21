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

    // Keep NSColorSampler alive on the main queue until AppKit has unwound
    // from this handler. The listener call below destroys `*this` and drops
    // the only strong reference to `color_sampler_`; releasing NSColorSampler
    // from inside its own selection handler wedges the system sampler on
    // macOS Tahoe. See https://crbug.com/509627745.
    NSColorSampler* sampler = weak_this->color_sampler_;
    dispatch_async(dispatch_get_main_queue(), ^{
      NS_VALID_UNTIL_END_OF_SCOPE NSColorSampler* sampler_keepalive = sampler;
    });

    if (selectedColor) {
      weak_this->listener_->ColorSelected(
          skia::CGColorRefToSkColor(selectedColor.CGColor));
    } else {
      weak_this->listener_->ColorSelectionCanceled();
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
