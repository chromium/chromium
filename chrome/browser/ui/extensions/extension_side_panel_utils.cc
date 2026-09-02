// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"

#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#endif

namespace extensions::side_panel_util {

namespace {

tabs::TabInterface* GetActiveTab(BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    return nullptr;
  }
  TabListInterface* tab_list = TabListInterface::From(browser_window);
  return tab_list ? tab_list->GetActiveTab() : nullptr;
}

SidePanelRegistry* GetTabRegistry(tabs::TabInterface* tab) {
  return tab ? SidePanelRegistry::From(tab) : nullptr;
}

SidePanelRegistry* GetTabRegistry(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  return GetTabRegistry(tabs::TabInterface::MaybeGetFromContents(web_contents));
}

// Returns true if `registry` exists and has an active entry whose key matches
// the provided `key`.
bool IsKeyActiveInRegistry(SidePanelRegistry* registry,
                           const SidePanelEntry::Key& key) {
  if (!registry) {
    return false;
  }
  auto entry = registry->GetActiveEntry();
  return entry.has_value() && entry.value()->key() == key;
}

}  // namespace

// Defined in extension_side_panel_utils.h
void ToggleExtensionSidePanel(BrowserWindowInterface* browser_window,
                              const ExtensionId& extension_id) {
  if (!browser_window) {
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* organizer_controller =
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()
          ? OrganizerPanelStateController::From(browser_window)
          : nullptr;
  if (organizer_controller) {
    organizer_controller->ToggleForExtension(extension_id);
    return;
  }
#endif

  SidePanelUI* side_panel_ui = SidePanelUI::From(browser_window);
  if (!side_panel_ui) {
    return;
  }

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
  side_panel_ui->Toggle(extension_key, SidePanelOpenTrigger::kExtension);
}

// Declared in extension_side_panel_utils.h
void OpenGlobalExtensionSidePanel(BrowserWindowInterface& browser_window,
                                  content::WebContents* web_contents,
                                  const ExtensionId& extension_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* organizer_controller =
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()
          ? OrganizerPanelStateController::From(&browser_window)
          : nullptr;
  if (organizer_controller) {
    organizer_controller->OpenForExtension(extension_id);
    return;
  }
#endif

  SidePanelUI* side_panel_ui = SidePanelUI::From(&browser_window);
  if (!side_panel_ui) {
    return;
  }

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
  tabs::TabInterface* active_tab = GetActiveTab(&browser_window);
  content::WebContents* active_web_contents =
      active_tab ? active_tab->GetContents() : nullptr;
  // If we're opening the side panel for the active tab, we can just call
  // `Show()` and be done with it.
  if (web_contents && active_web_contents == web_contents) {
    side_panel_ui->Show(extension_key, SidePanelOpenTrigger::kExtension);
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
    SidePanelRegistry* contextual_registry = GetTabRegistry(web_contents);
    if (contextual_registry && contextual_registry->GetActiveEntry()) {
      contextual_registry->ResetActiveEntry();
    }
  }

  // If the side panel isn't showing on the active tab, we can show the new
  // entry directly (since it's a global entry).
  if (!side_panel_ui->IsSidePanelShowing()) {
    side_panel_ui->Show(extension_key, SidePanelOpenTrigger::kExtension);
    return;
  }

  // The side panel is currently showing. This could be either:
  // 1) An active global side panel.
  // 2) An active contextual side panel.
  // In the case of a global side panel, we should override it. We don't want to
  // override a contextual side panel, though.
  SidePanelRegistry* active_tab_contextual_registry =
      GetTabRegistry(active_tab);
  bool has_active_contextual_entry =
      active_tab_contextual_registry &&
      active_tab_contextual_registry->GetActiveEntry().has_value();

  if (!has_active_contextual_entry) {
    // It must be an active global side panel. Call `Show()` to override it.
    side_panel_ui->Show(extension_key, SidePanelOpenTrigger::kExtension);
    return;
  }

  // There's an open contextual entry in the active tab. In this case, we set
  // the active global entry in the global registry, which will take effect
  // when a different tab activates.
  SidePanelRegistry* global_registry = SidePanelRegistry::From(&browser_window);
  if (!global_registry) {
    return;
  }
  SidePanelEntry* entry = global_registry->GetEntryForKey(extension_key);
  if (!entry) {
    return;
  }
  global_registry->SetActiveEntry(entry);
}

// Declared in extension_side_panel_utils.h
void OpenContextualExtensionSidePanel(BrowserWindowInterface& browser_window,
                                      content::WebContents& web_contents,
                                      const ExtensionId& extension_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* organizer_controller =
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()
          ? OrganizerPanelStateController::From(&browser_window)
          : nullptr;
  if (organizer_controller) {
    organizer_controller->OpenForExtension(extension_id);
    return;
  }
#endif

  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);

  tabs::TabInterface* active_tab = GetActiveTab(&browser_window);
  if (active_tab && active_tab->GetContents() == &web_contents) {
    SidePanelUI* side_panel_ui = SidePanelUI::From(&browser_window);
    if (side_panel_ui) {
      side_panel_ui->Show(extension_key, SidePanelOpenTrigger::kExtension);
    }
    return;
  }

  SidePanelRegistry* registry = GetTabRegistry(&web_contents);
  if (!registry) {
    return;
  }

  SidePanelEntry* entry = registry->GetEntryForKey(extension_key);
  if (!entry) {
    return;
  }
  registry->SetActiveEntry(entry);
}

// Declared in extension_side_panel_utils.h
void CloseGlobalExtensionSidePanel(BrowserWindowInterface* browser_window,
                                   const ExtensionId& extension_id) {
  if (!browser_window) {
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* organizer_controller =
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()
          ? OrganizerPanelStateController::From(browser_window)
          : nullptr;
  if (organizer_controller) {
    organizer_controller->CloseForExtension(extension_id);
    return;
  }
#endif

  SidePanelUI* side_panel_ui = SidePanelUI::From(browser_window);
  SidePanelEntry::Key extension_key =
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);

  tabs::TabInterface* active_tab = GetActiveTab(browser_window);
  SidePanelRegistry* active_contextual_registry = GetTabRegistry(active_tab);

  // If the global side panel entry for this extension is active, close it.
  if (active_contextual_registry &&
      active_contextual_registry->GetActiveEntry().has_value()) {
    // If the active web content contains a contextual panel and there is an
    // active global panel for this extension, reset the global side panel so it
    // doesn’t open when switching to any tab that doesn’t contain a contextual
    // panel (for example, a new tab).
    SidePanelRegistry* const global_registry =
        SidePanelRegistry::From(browser_window);
    if (IsKeyActiveInRegistry(global_registry, extension_key)) {
      global_registry->ResetActiveEntry();
    }
  } else {
    // Otherwise, if this extension's global side panel is visible,
    // simply close it.
    if (side_panel_ui &&
        side_panel_ui->IsSidePanelEntryShowing(extension_key)) {
      side_panel_ui->Close();
    }
  }
}

// Declared in extension_side_panel_utils.h
void CloseContextualExtensionSidePanel(BrowserWindowInterface* browser_window,
                                       content::WebContents* web_contents,
                                       const ExtensionId& extension_id) {
  if (!browser_window || !web_contents) {
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* organizer_controller =
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()
          ? OrganizerPanelStateController::From(browser_window)
          : nullptr;
  if (organizer_controller) {
    organizer_controller->CloseForExtension(extension_id);
    return;
  }
#endif

  const SidePanelEntry::Key extension_key(SidePanelEntry::Id::kExtension,
                                          extension_id);

  // Get the registry for the specific tab (whether active or inactive).
  SidePanelRegistry* contextual_registry = GetTabRegistry(web_contents);

  // Check if this extension is specifically active in this tab's registry.
  if (!IsKeyActiveInRegistry(contextual_registry, extension_key)) {
    return;
  }

  // Determine the active web contents in the window.
  tabs::TabInterface* active_tab = GetActiveTab(browser_window);
  content::WebContents* active_web_contents =
      active_tab ? active_tab->GetContents() : nullptr;

  SidePanelUI* side_panel_ui = SidePanelUI::From(browser_window);

  // If the provided web_contents refers to the active tab’s WebContents, and
  // the side panel in it was opened by this extension, then simply close the
  // side panel.
  if (web_contents == active_web_contents) {
    if (side_panel_ui &&
        side_panel_ui->IsSidePanelEntryShowing(extension_key)) {
      side_panel_ui->Close();
    }
    return;
  }

  // If an inactive tab is specified, reset that panel (so it doesn’t reopen
  // when you switch back to the tab).
  contextual_registry->ResetActiveEntry();
}

}  // namespace extensions::side_panel_util
