// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/page_transition_types.h"

class TabStripModel;

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController
//
//  An object that allows different types of ordering and reselection to be
//  heuristics plugged into a TabStripModel.
//
class TabStripModelOrderController : public TabStripModelObserver {
 public:
  explicit TabStripModelOrderController(TabStripModel* tabstrip);
  ~TabStripModelOrderController() override;

  // Determine where to place a newly opened tab by using the supplied
  // transition and foreground flag to figure out how it was opened.
  int DetermineInsertionIndex(ui::PageTransition transition,
                              bool foreground);

  // Determine where to shift selection after a tab is closed.
  base::Optional<int> DetermineNewSelectedIndex(int removed_index) const;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // Returns a valid index to be selected after the tab at |removing_index| is
  // closed. If |index| is after |removing_index|, |index| is adjusted to
  // reflect the fact that |removing_index| is going away.
  int GetValidIndex(int index, int removing_index) const;

  TabStripModel* tabstrip_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelOrderController);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H_
