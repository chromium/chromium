// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "ui/actions/actions.h"

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
  GlicSidePanelCoordinator(Profile* profile,
                           actions::ActionItem* root_action_item,
                           SidePanelCoordinator* side_panel_coordinator);
  ~GlicSidePanelCoordinator() override = default;

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 protected:
  // Called when the Glic enabled status changes for `profile_`.
  void OnGlicEnabledChanged();

  // `SidePanelEntryObserver`:
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  // Gets the Glic WebView from the Glic service.
  std::unique_ptr<views::View> CreateGlicWebView(SidePanelEntryScope& scope);
  raw_ptr<GlicKeyedService> glic_service_;
  raw_ptr<Profile> profile_;
  raw_ptr<actions::ActionItem> glic_action_;
  raw_ptr<SidePanelCoordinator> side_panel_coordinator_;
  base::CallbackListSubscription on_glic_enabled_changed_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
