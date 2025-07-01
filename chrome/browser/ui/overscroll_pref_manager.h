// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OVERSCROLL_PREF_MANAGER_H_
#define CHROME_BROWSER_UI_OVERSCROLL_PREF_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class TabStripModel;

class OverscrollPrefManager {
 public:
  OverscrollPrefManager(TabStripModel* tab_strip_model, bool is_type_devtools);
  ~OverscrollPrefManager();

  // Called to determine if the active tab of the hosting Browser can be
  // overscrolled with touch/wheel gestures.
  bool CanOverscrollContent() const;

 private:
  // Handle changes to kOverscrollHistoryNavigationEnabled preference.
  void OnOverscrollHistoryNavigationEnabledChanged();

  const raw_ptr<TabStripModel> tab_strip_model_;

  PrefChangeRegistrar local_state_pref_registrar_;

  // Whether this is for devtools type.
  const bool is_type_devtools_;

  // Whether the browser can overscroll content for history navigation.
  // Reflects the value of the kOverscrollHistoryNavigationEnabled pref.
  bool overscroll_history_navigation_enabled_ = true;
};

#endif  // CHROME_BROWSER_UI_OVERSCROLL_PREF_MANAGER_H_
