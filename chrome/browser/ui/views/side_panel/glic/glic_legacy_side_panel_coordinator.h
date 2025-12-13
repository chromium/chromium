// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_LEGACY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_LEGACY_SIDE_PANEL_COORDINATOR_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "ui/actions/actions.h"

class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace glic {

class GlicKeyedService;

// GlicLegacySidePanelCoordinator handles the creation and registration of the
// glic SidePanelEntry for the single instance side panel used when
// GlicMultiInstance flag is off (global panel scope).
class GlicLegacySidePanelCoordinator : public SidePanelEntryObserver {
 public:
  explicit GlicLegacySidePanelCoordinator(Browser* browser);
  ~GlicLegacySidePanelCoordinator() override = default;

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry(Browser* browser,
                              SidePanelRegistry* global_registry);

 protected:
  // Called when the Glic enabled status changes.
  void OnGlicEnabledChanged();

  // `SidePanelEntryObserver`:
  void OnEntryShown(SidePanelEntry* entry) override;

 private:
  // Gets the Glic WebView from the Glic service.
  std::unique_ptr<views::View> CreateGlicWebView(Browser* browser,
                                                 SidePanelEntryScope& scope);
  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<GlicKeyedService> glic_service_ = nullptr;
  raw_ptr<actions::ActionItem> glic_action_ = nullptr;
  base::CallbackListSubscription on_glic_enabled_changed_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_LEGACY_SIDE_PANEL_COORDINATOR_H_
