// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_

#include <vector>

#include "base/functional/function_ref.h"

class BrowserWindowInterface;

// Returns all browser windows.
// This is primarily used for features that need to operate on all browser
// windows at the same time. You should almost never be using this to find
// a specific browser window. There are some very rare exceptions, such as when
// you need to retrieve a browser window from an identifier or criteria when the
// caller is unassociated with that browser window (for instance, extensions
// modifying browser windows).
// Note that this doesn't account for any BrowserWindowInterfaces that are added
// or removed after the vector is returned.
std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces();

// Deprecated. Please use
// ForEachCurrentBrowserWindowInterfaceOrderedByActivation() instead.
std::vector<BrowserWindowInterface*>
GetBrowserWindowInterfacesOrderedByActivation();

void ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
    base::FunctionRef<void(BrowserWindowInterface*)> on_browser);

// Returns the last active browser window interface. This can be nullptr if
// there are no browser windows.
// CAUTION: This can return a browser window with *any* profile. Please verify
// the profile.
// If you only care whether a *particular* browser is active, prefer checking
// that with `browser->GetWindow()->IsActive()`, or similar.
BrowserWindowInterface* GetLastActiveBrowserWindowInterfaceWithAnyProfile();

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_
