// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"

namespace extensions::side_panel_util {

namespace {

// Returns true if `registry` exists and has an active entry whose key matches
// the provided `key`.
bool IsKeyActiveInRegistry(SidePanelRegistry* registry,
                           const SidePanelEntry::Key& key) {
  if (registry) {
    auto entry = registry->GetActiveEntryFor(
        ExtensionSidePanelCoordinator::GetPanelType());
    return entry.has_value() && entry.value()->key() == key;
  }
  return false;
}

}  // namespace

// Defined in extension_side_panel_utils.h
void ToggleExtensionSidePanel(BrowserWindowInterface* browser_window,
                              const ExtensionId& extension_id) {
  SidePanelUI* side_panel_ui = browser_window->GetFeatures().side_panel_ui();

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
  side_panel_ui->Toggle(extension_key, SidePanelOpenTrigger::kExtension);
}

// Declared in extension_side_panel_utils.h
void OpenGlobalExtensionSidePanel(BrowserWindowInterface& browser_window,
                                  content::WebContents* web_contents,
                                  const ExtensionId& extension_id) {
  SidePanelUI* side_panel_ui = browser_window.GetFeatures().side_panel_ui();

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
  content::WebContents* active_web_contents =
      browser_window.GetActiveTabInterface()->GetContents();
  // If we're opening the side panel for the active tab, we can just call
  // `Show()` and be done with it.
  if (web_contents && active_web_contents == web_contents) {
    side_panel_ui->Show(extension_key);
    return;
  }

  // Otherwise, we need to go through a few different steps. This flow is a
  // little complex because only extensions have this functionality. We could
  // move more of this logic into the SidePanelCoordinator if it makes sense to
  // in the future.
  if (web_contents) {
    // First, if there was a tab specified, check if there is an open
    // contextual panel in that tab. If there is one, we need to reset it so
    // that we can show the global entry instead.
    SidePanelRegistry* contextual_registry =
        SidePanelRegistry::GetDeprecated(web_contents);
    CHECK(contextual_registry);
    if (contextual_registry &&
        contextual_registry->GetActiveEntryFor(
            ExtensionSidePanelCoordinator::GetPanelType())) {
      contextual_registry->ResetActiveEntryFor(
          ExtensionSidePanelCoordinator::GetPanelType());
    }
  }

  // If the side panel isn't showing on the active tab, we can show the new
  // entry directly (since it's a global entry).
  if (!side_panel_ui->IsSidePanelShowing(
          ExtensionSidePanelCoordinator::GetPanelType())) {
    side_panel_ui->Show(extension_key);
    return;
  }

  // The side panel is currently showing. This could be either:
  // 1) An active global side panel.
  // 2) An active contextual side panel.
  // In the case of a global side panel, we should override it. We don't want to
  // override a contextual side panel, though.
  SidePanelRegistry* active_tab_contextual_registry =
      SidePanelRegistry::GetDeprecated(active_web_contents);
  CHECK(active_tab_contextual_registry);
  bool has_active_contextual_entry =
      active_tab_contextual_registry
          ->GetActiveEntryFor(ExtensionSidePanelCoordinator::GetPanelType())
          .has_value();

  if (!has_active_contextual_entry) {
    // It must be an active global side panel. Call `Show()` to override it.
    side_panel_ui->Show(extension_key);
    return;
  }

  // There's an open contextual entry in the active tab. In this case, we set
  // the active global entry in the global registry, which will take effect
  // when a different tab activates.
  SidePanelRegistry* global_registry = SidePanelRegistry::From(&browser_window);
  CHECK(global_registry);
  SidePanelEntry* entry = global_registry->GetEntryForKey(extension_key);
  CHECK(entry);
  global_registry->SetActiveEntry(entry);
}

// Declared in extension_side_panel_utils.h
void OpenContextualExtensionSidePanel(BrowserWindowInterface& browser_window,
                                      content::WebContents& web_contents,
                                      const ExtensionId& extension_id) {
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);

  if (browser_window.GetActiveTabInterface()->GetContents() == &web_contents) {
    browser_window.GetFeatures().side_panel_ui()->Show(extension_key);
    return;
  }

  SidePanelRegistry* registry = SidePanelRegistry::GetDeprecated(&web_contents);
  CHECK(registry);

  SidePanelEntry* entry = registry->GetEntryForKey(extension_key);
  CHECK(entry);
  registry->SetActiveEntry(entry);
}

// Declared in extension_side_panel_utils.h
void CloseGlobalExtensionSidePanel(BrowserWindowInterface* browser_window,
                                   const ExtensionId& extension_id) {
  SidePanelUI* side_panel_ui = browser_window->GetFeatures().side_panel_ui();
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);

  tabs::TabInterface* active_tab = browser_window->GetActiveTabInterface();
  SidePanelRegistry* active_contextual_registry =
      active_tab->GetTabFeatures()->side_panel_registry();

  // If the global side panel entry for this extension is active, close it.
  if (active_contextual_registry &&
      active_contextual_registry
          ->GetActiveEntryFor(ExtensionSidePanelCoordinator::GetPanelType())
          .has_value()) {
    // If the active web content contains a contextual panel and there is an
    // active global panel for this extension, reset the global side panel so it
    // doesn’t open when switching to any tab that doesn’t contain a contextual
    // panel (for example, a new tab).
    SidePanelRegistry* const global_registry =
        SidePanelRegistry::From(browser_window);
    if (IsKeyActiveInRegistry(global_registry, extension_key)) {
      global_registry->ResetActiveEntryFor(
          ExtensionSidePanelCoordinator::GetPanelType());
    }
  } else {
    // Otherwise, if this extension's global side panel is visible,
    // simply close it.
    if (side_panel_ui->IsSidePanelEntryShowing(extension_key)) {
      side_panel_ui->Close(ExtensionSidePanelCoordinator::GetPanelType());
    }
  }
}

// Declared in extension_side_panel_utils.h
void CloseContextualExtensionSidePanel(BrowserWindowInterface* browser_window,
                                       content::WebContents* web_contents,
                                       const ExtensionId& extension_id,
                                       std::optional<int> window_id) {
  const SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                          extension_id);

  // Determine the active web contents in the window.
  content::WebContents* active_web_contents =
      browser_window->GetActiveTabInterface()->GetContents();

  SidePanelUI* side_panel_ui = browser_window->GetFeatures().side_panel_ui();

  // If the provided web_contents refers to the active tab’s WebContents, and
  // the side panel in it was opened by this extension, then simply close the
  // side panel.
  if (web_contents == active_web_contents) {
    if (side_panel_ui->IsSidePanelEntryShowing(extension_key)) {
      side_panel_ui->Close(ExtensionSidePanelCoordinator::GetPanelType());
    }
    return;
  }

  // If an inactive tab is specified, check whether it has an open contextual
  // panel belonging to this extension. If it does, reset that panel (so it
  // doesn’t reopen when you switch back to the tab).
  if (web_contents) {
    tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
    SidePanelRegistry* contextual_registry =
        tab->GetTabFeatures()->side_panel_registry();
    if (IsKeyActiveInRegistry(contextual_registry, extension_key)) {
      contextual_registry->ResetActiveEntryFor(
          ExtensionSidePanelCoordinator::GetPanelType());
      return;
    }
  }

  // No contextual panel for this extension is open, close/reset global panel if
  // `window_id` is provided.
  if (window_id.has_value()) {
    CloseGlobalExtensionSidePanel(browser_window, extension_id);
  }
}

}  // namespace extensions::side_panel_util
