// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

// How much horizontal and vertical offset there is between newly
// opened windows.
const int WindowSizer::kWindowTilePixels = 22;
const int WindowSizer::kWindowMaxDefaultWidth = 1200;

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  NSRect work_area = NSScreen.mainScreen.visibleFrame;
  NSRect main_area = NSScreen.screens.firstObject.frame;
  NSPoint corner = NSMakePoint(NSMinX(work_area), NSMaxY(work_area));

  if (Browser* browser = chrome::FindLastActive()) {
    NSWindow* window = browser->window()->GetNativeWindow().GetNativeNSWindow();
    NSRect window_frame = [window frame];

    // Limit to not overflow the work area right and bottom edges.
    NSPoint limit = NSMakePoint(
        std::min(NSMinX(window_frame) + kWindowTilePixels,
                 NSMaxX(work_area) - size.width()),
        std::max(NSMaxY(window_frame) - kWindowTilePixels,
                 NSMinY(work_area) + size.height()));

    // Adjust corner to now overflow the work area left and top edges, so
    // that if a popup does not fit the title-bar is remains visible.
    corner = NSMakePoint(std::max(corner.x, limit.x),
                         std::min(corner.y, limit.y));
  }

  return gfx::Point(corner.x, NSHeight(main_area) - corner.y);
}
