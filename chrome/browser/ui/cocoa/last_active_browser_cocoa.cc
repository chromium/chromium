// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"

#include "chrome/browser/ui/browser_finder.h"

namespace chrome {

Browser* GetLastActiveBrowser() {
  return FindLastActive();
}

}  // namespace chrome
