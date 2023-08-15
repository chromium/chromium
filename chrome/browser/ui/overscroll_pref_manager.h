// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OVERSCROLL_PREF_MANAGER_H_
#define CHROME_BROWSER_UI_OVERSCROLL_PREF_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "components/prefs/pref_change_registrar.h"

class OverscrollPrefManager {
 public:
  explicit OverscrollPrefManager(Browser* browser);
  ~OverscrollPrefManager();

  bool IsOverscrollHistoryNavigationEnabled() const;

 private:
  // Handle changes to kOverscrollHistoryNavigationEnabled preference.
  void OnOverscrollHistoryNavigationEnabledChanged();

  // Browser that owns this object.
  const raw_ptr<Browser> browser_;

  PrefChangeRegistrar local_state_pref_registrar_;

  // Whether the browser can overscroll content for history navigation.
  // Reflects the value of the kOverscrollHistoryNavigationEnabled pref.
  bool overscroll_history_navigation_enabled_ = true;
};

#endif  // CHROME_BROWSER_UI_OVERSCROLL_PREF_MANAGER_H_
