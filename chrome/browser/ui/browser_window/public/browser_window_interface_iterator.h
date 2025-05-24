// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_

#include <vector>

class BrowserWindowInterface;

// Returns all browser windows.
// This is used for features that need to operate on all browser windows at the
// same time. Do not use this to find a specific browser window.
std::vector<BrowserWindowInterface*> GetAllBrowserWindowInterfaces();

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_ITERATOR_H_
