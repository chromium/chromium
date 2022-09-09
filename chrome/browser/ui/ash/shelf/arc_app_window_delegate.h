// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_DELEGATE_H_

// Delegate interface for ArcAppWindow.
class ArcAppWindowDelegate {
 public:
  ArcAppWindowDelegate() = default;
  ~ArcAppWindowDelegate() = default;

  ArcAppWindowDelegate(const ArcAppWindowDelegate&) = delete;
  ArcAppWindowDelegate& operator=(const ArcAppWindowDelegate&) = delete;

  // Returns the active task id.
  virtual int GetActiveTaskId() const = 0;

  // Returns the active session id for ARC ghost windows.
  virtual int GetActiveSessionId() const = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_DELEGATE_H_
