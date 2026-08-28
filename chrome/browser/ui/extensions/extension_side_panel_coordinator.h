// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/extension_icon_image.h"

class BrowserWindowInterface;
class SidePanelEntry;
class SidePanelEntryScope;
class SidePanelRegistry;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

// ExtensionSidePanelCoordinator handles the creation and registration of
// SidePanelEntries for the associated extension and creates the view to be
// shown if this extension's SidePanelEntry is active.
class ExtensionSidePanelCoordinator : public SidePanelService::Observer,
                                      public SidePanelEntryObserver,
                                      public ExtensionHostObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Creates the platform-specific ExtensionViewHost.
    virtual std::unique_ptr<ExtensionViewHost> CreateHost(const GURL& url) = 0;

    // Creates the platform-specific native view.
    virtual SidePanelNativeView CreateView(SidePanelEntryScope& scope) = 0;

    // Closes the active side panel view.
    virtual void CloseSidePanel(SidePanelEntry* entry) {}

    // Called when the entry is registered in the SidePanelRegistry.
    virtual void OnEntryRegistered() {}

    // Called when the entry is deregistered from the SidePanelRegistry.
    virtual void OnEntryDeregistered() {}

    // Called when the extension icon has been loaded or updated.
    virtual void OnIconUpdated() {}

    // Called when a tab-scoped coordinator's tab is about to leave its window.
    virtual void OnTabWillDetach(tabs::TabInterface* tab,
                                 tabs::TabInterface::DetachReason reason) {}

    // Called when a tab-scoped coordinator's tab is inserted into a window.
    virtual void OnTabDidInsert(tabs::TabInterface* tab) {}
  };

  explicit ExtensionSidePanelCoordinator(Profile* profile,
                                         BrowserWindowInterface* browser,
                                         tabs::TabInterface* tab_interface,
                                         const Extension* extension,
                                         SidePanelRegistry* registry,
                                         bool for_tab);
  ExtensionSidePanelCoordinator(const ExtensionSidePanelCoordinator&) = delete;
  ExtensionSidePanelCoordinator& operator=(
      const ExtensionSidePanelCoordinator&) = delete;
  ~ExtensionSidePanelCoordinator() override;

  static SidePanelType GetPanelType();

  // Creates the platform-specific delegate.
  static std::unique_ptr<Delegate> CreateDelegate(
      ExtensionSidePanelCoordinator* coordinator);

  // Returns the WebContents managed by `host_`.
  content::WebContents* GetHostWebContentsForTesting() const;

  // Deregisters this extension's SidePanelEntry from `registry_`.
  // To avoid re-entrancy this does not happen automatically in the destructor.
  void DeregisterEntry();

  // Called when the native view is destroyed.
  void OnViewDestroyed();

  Profile* profile() const { return profile_; }
  BrowserWindowInterface* browser() const { return browser_; }
  tabs::TabInterface* tab_interface() const { return tab_interface_; }
  const Extension* extension() const { return extension_; }
  ExtensionViewHost* host() const { return host_.get(); }
  IconImage* extension_icon() const { return extension_icon_.get(); }

  SidePanelEntryKey GetEntryKey() const;
  SidePanelEntry* GetEntry() const;
  BrowserWindowInterface* GetBrowser();

 private:
  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;

  // Dispatch the onOpened event when the panel is opened.
  void OnOpened();

  // Dispatches the onClosed event when the panel is closed or its WebContents
  // is destroyed. The event is only dispatched if the onOpened event was
  // dispatched prior.
  void OnClosed();

  bool IsGlobalCoordinator() const;

  // SidePanelService::Observer:
  void OnPanelOptionsChanged(
      const ExtensionId& extension_id,
      const api::side_panel::PanelOptions& updated_options) override;
  void OnSidePanelServiceShutdown() override;

  // ExtensionHostObserver:
  void OnExtensionHostDestroyed(ExtensionHost* host) override;
  void OnExtensionHostDidStopFirstLoad(const ExtensionHost* host) override;

  // Creates and registers the SidePanelEntry for this extension, and observes
  // the entry.
  void CreateAndRegisterEntry();

  // Creates a view for the extension's resource URL. This is called when this
  // extension's SidePanelEntry is about to be shown in the side panel and a
  // view for the entry has not been cached.
  SidePanelNativeView CreateView(SidePanelEntryScope& scope);

  // Called when window.close() is called from the extension's side panel page.
  // This closes the side panel if the extension's panel is showing. Otherwise
  // it clears the extension entry's cached view.
  void HandleCloseExtensionSidePanel(ExtensionHost* host);

  // Loads the `side_panel_url_` into the WebContents of the view for the
  // extension's SidePanelEntry. To avoid unnecessary updates, this is only
  // called when this extension's SidePanelEntry is currently active.
  void NavigateIfNecessary();

  // Loads the extension's icon for its SidePanelEntry.
  void LoadExtensionIcon();

  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // The profile associated with either `browser_` or `web_contents_`.
  raw_ptr<Profile> profile_;

  // The browser that owns `registry_` and the ExtensionSidePanelManager that
  // owns this class. A reference for this is kept so the side panel can be
  // closed when window.close() is called from the extension's side panel page.
  // Only one of `browser_` or `tab_interface_` should be defined.
  raw_ptr<BrowserWindowInterface> browser_;

  // The TabInterface that owns `registry_` and the ExtensionSidePanelManager
  // that owns this class. Refer to the comment for `browser_` on why this
  // reference needs to be kept.
  raw_ptr<tabs::TabInterface> tab_interface_;

  // The extension that registered the side panel content that's managed by this
  // class.
  raw_ptr<const Extension> extension_;

  // The SidePanelRegistry that lives in the same user data that an instance of
  // this class lives in. Owns all extension entries managed by `coordinators_`.
  raw_ptr<SidePanelRegistry> registry_;

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

  // Track whether the side panel is currently active for this entry.
  bool is_panel_active_ = false;

  // Track whether the onOpened event has been dispatched.
  bool on_opened_dispatched_ = false;

  // The ID of the browser window in which the panel is shown.
  std::optional<int> window_id_;

  // Whether this coordinator is tab-scoped or window-scoped.
  const bool for_tab_;

  // Optional platform-specific delegate.
  std::unique_ptr<Delegate> delegate_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Scoped observations for UI and extension backend components.
  base::ScopedObservation<SidePanelService, SidePanelService::Observer>
      scoped_service_observation_{this};
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      scoped_entry_observation_{this};
  base::ScopedObservation<ExtensionHost, ExtensionHostObserver>
      scoped_host_observation_{this};

  // Must be the last member.
  base::WeakPtrFactory<ExtensionSidePanelCoordinator> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_COORDINATOR_H_
