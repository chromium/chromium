// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_SIDE_PANEL_COORDINATOR_H_
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ui/gfx/image/image.h"

class Browser;
class SidePanelCoordinator;

// LensSidePanelCoordinator handles the creation and registration of the
// LensUnifiedSidePanelEntry.
class LensSidePanelCoordinator
    : public BrowserUserData<LensSidePanelCoordinator>,
      public SidePanelViewStateObserver,
      public SidePanelEntryObserver,
      public TemplateURLServiceObserver {
 public:
  explicit LensSidePanelCoordinator(Browser* browser);
  ~LensSidePanelCoordinator() override;

  // Registers lens entry in the side panel and shows side panel with lens
  // selected if its not shown
  void RegisterEntryAndShow(const content::OpenURLParams& params);

  content::WebContents* GetViewWebContentsForTesting();

  // Gets the URL needed to open the current side panel results in a new tab.
  // Returns an empty URL if the view is null or results have not yet loaded.
  GURL GetOpenInNewTabURL() const;

  bool OpenResultsInNewTabForTesting();

  bool IsLaunchButtonEnabledForTesting();

 private:
  friend class BrowserUserData<LensSidePanelCoordinator>;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  bool IsDefaultSearchProviderGoogle();

  // Get the label to display in the side panel combobox dropdown.
  std::u16string GetComboboxLabel();

  // Get the favicon to display in the side panel combobox dropdown.
  const ui::ImageModel GetFaviconImage();

  // Get the lens action item that shows the lens sidepanel.
  actions::ActionItem* GetActionItem();

  // Updates the text and image of the lens sidepanel action item.
  void UpdateActionItem();

  // This is a callback called after fetching favicon from favicon_cache.
  void OnFaviconFetched(const gfx::Image& favicon);

  BrowserView* GetBrowserView();

  SidePanelCoordinator* GetSidePanelCoordinator();

  // Removes the lens entry from the side panel.
  void DeregisterLensFromSidePanel();

  // Forces an update of the enabled/disabled state of the new tab button on the
  // side panel header based on the new tab URL becoming available. If the new
  // tab URL is valid it enables the button, otherwise it is disabled.
  void UpdateNewTabButtonState();

  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override;

  // SidePanelViewStateObserver
  void OnSidePanelDidClose() override;

  std::unique_ptr<views::View> CreateLensWebView(
      const content::OpenURLParams& params);

  raw_ptr<TemplateURLService> template_url_service_;
  base::WeakPtr<lens::LensUnifiedSidePanelView> lens_side_panel_view_;
  raw_ptr<const TemplateURL> current_default_search_provider_;
  std::unique_ptr<FaviconCache> favicon_cache_;

  base::WeakPtrFactory<LensSidePanelCoordinator> weak_ptr_factory_{this};

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_SIDE_PANEL_COORDINATOR_H_
