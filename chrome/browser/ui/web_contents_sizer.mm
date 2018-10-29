// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_contents_sizer.h"

#import <Cocoa/Cocoa.h>

#include "content/public/browser/web_contents.h"

void ResizeWebContents(content::WebContents* web_contents,
                       const gfx::Rect& new_bounds) {
  NSView* view = web_contents->GetNativeView().GetNativeNSView();
  NSRect old_wcv_frame = [view frame];
  CGFloat new_x = old_wcv_frame.origin.x;
  CGFloat new_y =
      old_wcv_frame.origin.y
      + (old_wcv_frame.size.height - new_bounds.size().height());
  NSRect new_wcv_frame =
      NSMakeRect(new_x, new_y,
                 new_bounds.size().width(),
                 new_bounds.size().height());
  [view setFrame:new_wcv_frame];
}
