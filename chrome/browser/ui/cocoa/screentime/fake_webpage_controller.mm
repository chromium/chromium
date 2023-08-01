// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/fake_webpage_controller.h"

#import <Cocoa/Cocoa.h>

namespace {

NSView* MakeView(bool enabled) {
  NSView* view = [[NSView alloc] init];
  view.wantsLayer = YES;
  view.layer.backgroundColor = NSColor.blueColor.CGColor;
  view.hidden = !enabled;
  return view;
}

}  // namespace

namespace screentime {

FakeWebpageController::FakeWebpageController(
    const BlockedChangedCallback& blocked_changed_callback)
    : view_(MakeView(enabled_)),
      blocked_changed_callback_(blocked_changed_callback) {}
FakeWebpageController::~FakeWebpageController() = default;

NSView* FakeWebpageController::GetView() {
  return view_;
}

void FakeWebpageController::PageURLChangedTo(const GURL& url) {
  visited_urls_.push_back(url);

  enabled_ = !enabled_;
  [view_ setHidden:!enabled_];
  blocked_changed_callback_.Run(enabled_);
}

}  // namespace screentime
