// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"

namespace extensions::side_panel_util {

// Defined in extension_side_panel_utils.h
void ToggleExtensionSidePanel(Browser* browser,
                              const ExtensionId& extension_id) {
  SidePanelUI* side_panel_ui = browser->GetFeatures().side_panel_ui();

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
  side_panel_ui->Toggle(extension_key, SidePanelOpenTrigger::kExtension);
}

// Declared in extension_side_panel_utils.h
void OpenGlobalExtensionSidePanel(Browser& browser,
                                  content::WebContents* web_contents,
                                  const ExtensionId& extension_id) {
  SidePanelUI* side_panel_ui = browser.GetFeatures().side_panel_ui();

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
  content::WebContents* active_web_contents =
      browser.tab_strip_model()->GetActiveWebContents();
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
    if (contextual_registry && contextual_registry->active_entry()) {
      contextual_registry->ResetActiveEntry();
    }
  }

  // If the side panel isn't showing on the active tab, we can show the new
  // entry directly (since it's a global entry).
  if (!side_panel_ui->IsSidePanelShowing()) {
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
      active_tab_contextual_registry->active_entry().has_value();

  if (!has_active_contextual_entry) {
    // It must be an active global side panel. Call `Show()` to override it.
    side_panel_ui->Show(extension_key);
    return;
  }

  // There's an open contextual entry in the active tab. In this case, we set
  // the active global entry in the global registry, which will take effect
  // when a different tab activates.
  SidePanelRegistry* global_registry =
      browser.GetFeatures().side_panel_coordinator()->GetWindowRegistry();
  CHECK(global_registry);
  SidePanelEntry* entry = global_registry->GetEntryForKey(extension_key);
  CHECK(entry);
  global_registry->SetActiveEntry(entry);
}

// Declared in extension_side_panel_utils.h
void OpenContextualExtensionSidePanel(Browser& browser,
                                      content::WebContents& web_contents,
                                      const ExtensionId& extension_id) {
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);

  if (browser.tab_strip_model()->GetActiveWebContents() == &web_contents) {
    browser.GetFeatures().side_panel_ui()->Show(extension_key);
    return;
  }

  SidePanelRegistry* registry = SidePanelRegistry::GetDeprecated(&web_contents);
  CHECK(registry);

  SidePanelEntry* entry = registry->GetEntryForKey(extension_key);
  CHECK(entry);
  registry->SetActiveEntry(entry);
}

}  // namespace extensions::side_panel_util
