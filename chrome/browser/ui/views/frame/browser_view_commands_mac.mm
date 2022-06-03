// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/browser_view_commands_mac.h"

#import <AppKit/AppKit.h>

#include "base/notreached.h"
#include "chrome/app/chrome_command_ids.h"

void ForwardCutCopyPasteToNSApp(int command_id) {
  if (command_id == IDC_CUT)
    [NSApp sendAction:@selector(cut:) to:nil from:nil];
  else if (command_id == IDC_COPY)
    [NSApp sendAction:@selector(copy:) to:nil from:nil];
  else if (command_id == IDC_PASTE)
    [NSApp sendAction:@selector(paste:) to:nil from:nil];
  else
    NOTREACHED();
}
