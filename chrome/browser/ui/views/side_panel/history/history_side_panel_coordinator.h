// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_HISTORY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_HISTORY_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class BrowserWindowInterface;
class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}

// HistorySidePanelCoordinator handles the creation and registration of
// the history SidePanelEntry.
class HistorySidePanelCoordinator {
 public:
  explicit HistorySidePanelCoordinator(BrowserWindowInterface* browser);
  ~HistorySidePanelCoordinator() = default;

  // Returns whether HistorySidePanelCoordinator is supported.
  // If this returns false, it should not be registered with the side
  // panel registry.
  static bool IsSupported();

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  std::unique_ptr<views::View> CreateHistoryWebView(SidePanelEntryScope& scope);

  // Shows the History side panel with `query` pre-populated. Returns true if
  // this was successful.
  bool Show(const std::string& query);

  // Toggles the registration of the Journeys in the side panel based on
  // Journeys preferences
  void OnHistoryClustersPreferenceChanged();

 private:
  raw_ptr<BrowserWindowInterface> browser_;

  // Used to store the initial query for the next-created WebUI instance.
  std::string initial_query_;

  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_HISTORY_SIDE_PANEL_COORDINATOR_H_
