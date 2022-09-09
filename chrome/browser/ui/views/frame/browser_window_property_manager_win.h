// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_PROPERTY_MANAGER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_PROPERTY_MANAGER_WIN_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class BrowserView;

// This class is responsible for updating the app id and relaunch details of a
// browser frame.
class BrowserWindowPropertyManager {
 public:
  BrowserWindowPropertyManager(const BrowserWindowPropertyManager&) = delete;
  BrowserWindowPropertyManager& operator=(const BrowserWindowPropertyManager&) =
      delete;
  virtual ~BrowserWindowPropertyManager();

  static std::unique_ptr<BrowserWindowPropertyManager>
  CreateBrowserWindowPropertyManager(const BrowserView* view, HWND hwnd);

 private:
  BrowserWindowPropertyManager(const BrowserView* view, HWND hwnd);

  void UpdateWindowProperties();
  void OnProfileIconVersionChange();

  PrefChangeRegistrar profile_pref_registrar_;
  raw_ptr<const BrowserView> view_;
  const HWND hwnd_;

};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_PROPERTY_MANAGER_WIN_H_
