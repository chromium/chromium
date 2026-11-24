// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_VIEW_CUJ_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_VIEW_CUJ_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserWindowInterface;

// Emits Critical User Journey (CUJ) custom events when the user enters Split
// View, sustains a populated two-page comparison past the accidental-open
// floor, or exits early / on an unpopulated NTP.
class SplitViewCujController : public TabStripModelObserver {
 public:
  static constexpr base::TimeDelta kDefaultSustainedComparisonDelay =
      base::Seconds(15);

  explicit SplitViewCujController(
      BrowserWindowInterface* browser_window_interface);
  ~SplitViewCujController() override;

  SplitViewCujController(const SplitViewCujController&) = delete;
  SplitViewCujController& operator=(const SplitViewCujController&) = delete;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;
  void WillCloseAllTabs(TabStripModel* tab_strip_model) override;
  void CloseAllTabsStopped(TabStripModel* tab_strip_model,
                           CloseAllStoppedReason reason) override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  bool is_closing_all_tabs_ = false;
};

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_VIEW_CUJ_CONTROLLER_H_
