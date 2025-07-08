// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

#include "base/notimplemented.h"

std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces() {
  // TODO(https://crbug.com/419057482): Implement this once we have an
  // Android variant of BrowserWindowInterface.
  NOTIMPLEMENTED();
  return {};
}

std::vector<BrowserWindowInterface*>
GetBrowserWindowInterfacesOrderedByActivation() {
  // TODO(https://crbug.com/419057482): Implement this once we have an
  // Android variant of BrowserWindowInterface.
  NOTIMPLEMENTED();
  return {};
}
