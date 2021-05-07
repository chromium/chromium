// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_SIDE_PANEL_CONTROLLER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class Extension;
}

namespace views {
class WebView;
}  // namespace views

class BrowserView;
class SidePanel;
class ToolbarButton;

// A class that manages hosting the extension WebContents in the left aligned
// side panel of the browser window.
// TODO(crbug.com/1197555): Remove this once the experiment has concluded.
class ExtensionsSidePanelController
    : public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public extensions::ExtensionRegistryObserver {
 public:
  ExtensionsSidePanelController(SidePanel* side_panel,
                                BrowserView* browser_view);
  ExtensionsSidePanelController(const ExtensionsSidePanelController&) = delete;
  ExtensionsSidePanelController& operator=(
      const ExtensionsSidePanelController&) = delete;
  ~ExtensionsSidePanelController() override;

  std::unique_ptr<ToolbarButton> CreateToolbarButton();
  void ResetWebContents();
  void SetNewWebContents();

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  views::WebView* get_web_view_for_testing() { return web_view_; }

 private:
  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  const extensions::Extension* GetExtension();

  void SidePanelButtonPressed();

  const extensions::ExtensionId extension_id_;
  SidePanel* side_panel_;
  BrowserView* browser_view_;
  views::WebView* web_view_;
  std::unique_ptr<content::WebContents> web_contents_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      registry_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_SIDE_PANEL_CONTROLLER_H_
