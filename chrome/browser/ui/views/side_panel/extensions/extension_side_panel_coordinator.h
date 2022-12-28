// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"

class Browser;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace extensions {

class Extension;

// ExtensionSidePanelCoordinator handles the creation and registration of
// SidePanelEntries for the associated extension and creates the view to be
// shown if this extension's SidePanelEntry is active.
class ExtensionSidePanelCoordinator : public ExtensionViewViews::Observer {
 public:
  explicit ExtensionSidePanelCoordinator(Browser* browser,
                                         const Extension* extension,
                                         SidePanelRegistry* global_registry);
  ExtensionSidePanelCoordinator(const ExtensionSidePanelCoordinator&) = delete;
  ExtensionSidePanelCoordinator& operator=(
      const ExtensionSidePanelCoordinator&) = delete;
  ~ExtensionSidePanelCoordinator() override;

 private:
  // ExtensionViewViews::Observer
  void OnViewDestroying() override;

  // Creates and registers the SidePanelEntry for this extension. This is called
  // if the extension has a default side panel path when the browser view is
  // created or when the extension is loaded.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry,
                              const GURL& side_panel_url);

  // Creates and transfers ownership of a view for the extension's resource URL
  // `side_panel_url`. This is called when this extension's SidePanelEntry is
  // about to be shown in the side panel and a view for the entry has not been
  /// cached.
  std::unique_ptr<views::View> CreateView(const GURL& side_panel_url);

  raw_ptr<Browser> browser_;
  const Extension* extension_;

  // The ExtensionViewHost that backs the view in the side panel for this
  // extension. This is defined if and only if the aforementioned view exists.
  // Note: the view is destroyed when the side panel is closed or when the
  // SidePanelEntry for this extension is deregistered.
  std::unique_ptr<ExtensionViewHost> host_ = nullptr;

  base::ScopedObservation<ExtensionViewViews, ExtensionViewViews::Observer>
      scoped_view_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
