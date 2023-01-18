// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "extensions/browser/extension_icon_image.h"

class Browser;
class SidePanelRegistry;

namespace content {
class WebContents;
}

namespace views {
class View;
}  // namespace views

namespace extensions {

class Extension;

// ExtensionSidePanelCoordinator handles the creation and registration of
// SidePanelEntries for the associated extension and creates the view to be
// shown if this extension's SidePanelEntry is active.
class ExtensionSidePanelCoordinator : public ExtensionViewViews::Observer,
                                      public IconImage::Observer,
                                      public SidePanelService::Observer {
 public:
  explicit ExtensionSidePanelCoordinator(Browser* browser,
                                         const Extension* extension,
                                         SidePanelRegistry* global_registry);
  ExtensionSidePanelCoordinator(const ExtensionSidePanelCoordinator&) = delete;
  ExtensionSidePanelCoordinator& operator=(
      const ExtensionSidePanelCoordinator&) = delete;
  ~ExtensionSidePanelCoordinator() override;

  // Returns the WebContents managed by `host_`.
  content::WebContents* GetHostWebContentsForTesting() const;

  // Calls LoadExtensionIcon() again. Since LoadExtensionIcon() is called right
  // when this class is created, it's difficult for tests to catch the
  // OnExtensionIconImageChanged event. This method allows tests to initiate
  // and wait for that event.
  void LoadExtensionIconForTesting();

 private:
  SidePanelEntry::Key GetEntryKey() const;

  // Deregisters this extension's SidePanelEntry from the global
  // SidePanelCoordinator.
  void DeregisterGlobalEntry();

  // SidePanelService::Observer:
  void OnPanelOptionsChanged(
      const ExtensionId& extension_id,
      const api::side_panel::PanelOptions& updated_options) override;
  void OnSidePanelServiceShutdown() override;

  // ExtensionViewViews::Observer
  void OnViewDestroying() override;

  // IconImage::Observer
  void OnExtensionIconImageChanged(IconImage* image) override;

  // Creates and registers the SidePanelEntry for this extension, and observes
  // the entry. This is called if the extension has a default side panel path
  // when the browser view is created or when the extension is loaded.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // Creates a view for the extension's resource URL. This is called when this
  // extension's SidePanelEntry is about to be shown in the side panel and a
  // view for the entry has not been cached.
  std::unique_ptr<views::View> CreateView();

  // Loads the `side_panel_url_` into the WebContents of the view for the
  // extension's SidePanelEntry. To avoid unnecessary updates, this is only
  // called when this extension's SidePanelEntry is currently active.
  void NavigateIfNecessary();

  // Loads the extension's icon for its SidePanelEntry.
  void LoadExtensionIcon();

  raw_ptr<Browser> browser_;
  raw_ptr<const Extension> extension_;

  // The current URL set for the extension's global side panel. This is set in
  // the constructor or during OnPanelOptionsChanged.
  GURL side_panel_url_;

  // The ExtensionViewHost that backs the view in the side panel for this
  // extension. This is defined if and only if the aforementioned view exists.
  // Note: the view is destroyed when the side panel is closed or when the
  // SidePanelEntry for this extension is deregistered.
  std::unique_ptr<ExtensionViewHost> host_ = nullptr;

  // The extension's own icon for its side panel entry.
  std::unique_ptr<IconImage> extension_icon_;

  base::ScopedObservation<ExtensionViewViews, ExtensionViewViews::Observer>
      scoped_view_observation_{this};
  base::ScopedObservation<SidePanelService, SidePanelService::Observer>
      scoped_service_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
