// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_OBSERVER_H_

#include "chrome/browser/ui/views/chrome_views_export.h"

class TabStrip;

////////////////////////////////////////////////////////////////////////////////
//
// TabStripObserver
//
//  Objects implement this interface when they wish to be notified of changes
//  to the TabStrip.
//
//  Register your TabStripObserver with the TabStrip using its
//  Add/RemoveObserver methods.
//
////////////////////////////////////////////////////////////////////////////////
class CHROME_VIEWS_EXPORT TabStripObserver {
 public:
  // Sent when a new tab has been added at |index|.
  virtual void OnTabAdded(int index);

  // Sent when the tab at |from_index| has been moved to |to_index|.
  virtual void OnTabMoved(int from_index, int to_index);

  // Sent when the tab at |index| has been removed.
  virtual void OnTabRemoved(int index);

 protected:
  virtual ~TabStripObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_OBSERVER_H_
