// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/policy/enterprise_startup_dialog_mac_util.h"

#include <Cocoa/Cocoa.h>

namespace policy {

void StartModal(gfx::NativeWindow dialog) {
  [NSApp runModalForWindow:dialog.GetNativeNSWindow()];
}
void StopModal() {
  [NSApp stopModal];
}

}  // namespace policy
