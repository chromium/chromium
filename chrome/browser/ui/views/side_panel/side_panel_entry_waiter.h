// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_WAITER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_WAITER_H_

#include <optional>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

// This class uses the SidePanelContentProxy to wait for the SidePanelEntry's
// content view to be ready to be shown.
class SidePanelEntryWaiter {
 public:
  using PopulateSidePanelCallback = base::OnceCallback<void(
      SidePanelEntry* entry,
      std::optional<std::unique_ptr<views::View>> content_view)>;

  SidePanelEntryWaiter();
  ~SidePanelEntryWaiter();

  // Calling this method cancels all previous calls to this method.
  // If the entry is destroyed while waiting, the callback is not invoked.
  void WaitForEntry(SidePanelEntry* entry, PopulateSidePanelCallback callback);

  void ResetLoadingEntryIfNecessary();

  void SetNoDelaysForTesting(bool no_delays_for_testing);

  SidePanelEntry* loading_entry() const { return loading_entry_.get(); }

 private:
  void RunLoadedCallback(PopulateSidePanelCallback callback);

  // When true, don't delay switching panels.
  bool show_immediately_for_testing_ = false;

  // Tracks the entry that is loading.
  base::WeakPtr<SidePanelEntry> loading_entry_;

  // This class will load at most one entry at a time. If a new one is
  // requested, the old one is canceled automatically.
  base::CancelableOnceClosure loaded_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_WAITER_H_
