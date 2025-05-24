// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager_unittest_mac_helper.h"

#include <Foundation/Foundation.h>

#include "ui/gfx/native_widget_types.h"

namespace {
NSMutableSet* ObjectSetForTesting() {
  static NSMutableSet* set = [NSMutableSet set];
  return set;
}
}  // namespace

gfx::NativeWindow FakeNativeWindowForTesting() {
  id an_object = [[NSObject alloc] init];
  [ObjectSetForTesting() addObject:an_object];  // IN-TEST
  return gfx::NativeWindow(an_object);
}

void TearDownFakeNativeWindowsForTesting() {
  [ObjectSetForTesting() removeAllObjects];  // IN-TEST
}
