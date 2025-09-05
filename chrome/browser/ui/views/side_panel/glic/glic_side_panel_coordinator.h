// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

class Profile;

class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace glic {

class GlicKeyedService;

// GlicSidePanelCoordinator handles the creation and registration of the
// glic SidePanelEntry.
class GlicSidePanelCoordinator : public SidePanelEntryObserver {
 public:
  explicit GlicSidePanelCoordinator(Profile* profile);
  ~GlicSidePanelCoordinator() override = default;

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // `SidePanelEntryObserver`:
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  // Gets the Glic WebView from the Glic service.
  std::unique_ptr<views::View> CreateGlicWebView(SidePanelEntryScope& scope);

  raw_ptr<GlicKeyedService> glic_service_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
