// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_

#include <memory>
#include <vector>

#include "base/supports_user_data.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

class BrowserWindowInterface;
class SidePanelCoordinator;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

// This class is used for storing SidePanelEntries specific to a context. This
// context can be one per tab or one per window. See also SidePanelCoordinator.
class SidePanelRegistry final : public SidePanelEntryObserver {
 public:
  explicit SidePanelRegistry(tabs::TabInterface* tab_interface);
  explicit SidePanelRegistry(BrowserWindowInterface* browser_window_interface);
  SidePanelRegistry(const SidePanelRegistry&) = delete;
  SidePanelRegistry& operator=(const SidePanelRegistry&) = delete;
  ~SidePanelRegistry() override;

  // The tab-scoped registry should be obtained from the tab, e.g.
  // tab_model->tab_features()->side_panel_registry(). This is the fallback for
  // old code that is conceptually tab-scoped but does not use tab_model.
  //
  // Gets the contextual registry for the tab associated with |web_contents|.
  // Can return null for non-tab contents.
  static SidePanelRegistry* GetDeprecated(content::WebContents* web_contents);

  SidePanelEntry* GetEntryForKey(const SidePanelEntry::Key& entry_key);
  void ResetActiveEntry();
  void ResetLastActiveEntry();

  // Clear cached view for all owned entries.
  void ClearCachedEntryViews();

  // Registers a SidePanelEntry. Returns true if the entry is successfully
  // registered and false if a SidePanelEntry already exists in the registry for
  // the provided SidePanelEntry::Id.
  bool Register(std::unique_ptr<SidePanelEntry> entry);

  // Deregisters the entry for the given SidePanelEntry::Key. Returns true if
  // successful and false if there is no entry registered for the `key`.
  bool Deregister(const SidePanelEntry::Key& key);

  // Deregisters the entry for the given SidePanelEntry::Key and returns the
  // entry or nullptr if one does not exist.
  std::unique_ptr<SidePanelEntry> DeregisterAndReturnEntry(
      const SidePanelEntry::Key& key);

  // Set the active entry in the side panel to be |entry|.
  void SetActiveEntry(SidePanelEntry* entry);

  std::optional<SidePanelEntry*> active_entry() { return active_entry_; }
  std::optional<SidePanelEntry*> last_active_entry() {
    return last_active_entry_;
  }
  std::vector<std::unique_ptr<SidePanelEntry>>& entries() { return entries_; }

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* id) override;

 private:
  SidePanelCoordinator* GetCoordinator();

  std::unique_ptr<SidePanelEntry> RemoveEntry(SidePanelEntry* entry);

  // The active entry hosted in the side panel used to determine what entry
  // should be visible. This is reset by the coordinator when the panel is
  // closed. When there are multiple registries, this may not be the entry
  // currently visible in the side panel.
  std::optional<SidePanelEntry*> active_entry_;

  // The last active entry hosted in the side panel before it was closed. This
  // is set when the active entry is reset i.e. when the panel is closed.
  std::optional<SidePanelEntry*> last_active_entry_;

  std::vector<std::unique_ptr<SidePanelEntry>> entries_;

  const std::variant<tabs::TabInterface*, BrowserWindowInterface*> owner_;

  std::optional<SidePanelEntryKey> deregistering_entry_key_ = std::nullopt;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_REGISTRY_H_
