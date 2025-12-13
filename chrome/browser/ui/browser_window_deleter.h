// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_DELETER_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_DELETER_H_

class BrowserWindow;

// Custom deleter to give BrowserWindow subclasses greater flexibility in how
// BrowserWindow instances are destroyed.
struct BrowserWindowDeleter {
  void operator()(BrowserWindow* browser_window);
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_DELETER_H_
