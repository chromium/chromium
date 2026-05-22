// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_DEFERRED_ENTRY_TRACKER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_DEFERRED_ENTRY_TRACKER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_base.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

// Tracks deferred side panel entries. A "deferred" entry is an entry that could
// have been shown, but was deferred due to Android constraints such as narrow
// window size.
//
// Example:
//
// We save a deferred entry when:
// (1) the window becomes too narrow (this captures the current tab), and
// (2) at Show() time (this captures any attempt to show an entry _after_ the
//     window became small, such as during tab switches).
//
// We remove and show a deferred entry when:
// (1) the window becomes wide enough again, or
// (2) at tab switch time.
// These are entry points to showing an entry _without_ an explicit entry key
// from a feature.
class SidePanelDeferredEntryTracker {
 public:
  explicit SidePanelDeferredEntryTracker(BrowserWindowInterface* browser);
  ~SidePanelDeferredEntryTracker();

  SidePanelDeferredEntryTracker(const SidePanelDeferredEntryTracker&) = delete;
  SidePanelDeferredEntryTracker& operator=(
      const SidePanelDeferredEntryTracker&) = delete;

  // Adds the active entries from the window-scoped registry and the active
  // tab's tab-scoped registry as deferred entries.
  // This will also reset the active entry in the corresponding registries.
  void AddActiveEntries();

  // Adds the given entry key as deferred.
  void AddEntry(const SidePanelUIBase::UniqueKey& key);

  // Returns the deferred entry for the given tab if one exists, or
  // std::nullopt if there is none.
  //
  // The entry for a tab can be either tab-scoped or window-scoped, and the
  // priority is: tab-scoped deferred entry > window-scoped deferred entry.
  std::optional<SidePanelUIBase::UniqueKey> GetEntry(
      const tabs::TabHandle& tab_handle) const;

  // Clears the deferred state for the given entry `key`.
  void ClearEntry(const SidePanelUIBase::UniqueKey& key);

  // Clears any deferred tab-scoped entry associated with `tab_handle`.
  void ClearEntryForTab(const tabs::TabHandle& tab_handle);

 private:
  const raw_ptr<BrowserWindowInterface> browser_;

  // Maps a specific tab to the side panel entry that was deferred.
  absl::flat_hash_map<tabs::TabHandle, SidePanelEntryKey>
      tab_scoped_deferred_entries_;

  // Tracks the window scoped side panel entry that was deferred.
  std::optional<SidePanelEntryKey> window_scoped_deferred_entry_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_DEFERRED_ENTRY_TRACKER_H_
