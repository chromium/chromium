// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LAST_ACTIVE_BROWSER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_LAST_ACTIVE_BROWSER_COCOA_H_

#include "build/build_config.h"

// This file exists only to provide a C function that BrowserList can friend.
// Access to this function is legitimately needed from a variety of places in
// the Cocoa frontend that cannot be allowed via C++ friendship as these places
// are in Obj-C objects.
// Do NOT include or build this file on non-Mac platforms.
#if !BUILDFLAG(IS_MAC)
#error This file is intended for use in the Cocoa frontend only.
#endif

class Browser;

namespace chrome {

Browser* GetLastActiveBrowser();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_LAST_ACTIVE_BROWSER_COCOA_H_
