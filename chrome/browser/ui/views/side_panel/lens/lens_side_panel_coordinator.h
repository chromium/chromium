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
#include "components/search_engines/template_url_service_observer.h"

class Browser;

// LensSidePanelCoordinator handles the creation and registration of the
// LensUnifiedSidePanelEntry.
class LensSidePanelCoordinator
    : public BrowserUserData<LensSidePanelCoordinator>,
      public SidePanelViewStateObserver,
      public SidePanelEntryObserver,
      public TemplateURLServiceObserver {
 public:
  explicit LensSidePanelCoordinator(Browser* browser);
  LensSidePanelCoordinator(const LensSidePanelCoordinator&) = delete;
  LensSidePanelCoordinator& operator=(const LensSidePanelCoordinator&) = delete;
  ~LensSidePanelCoordinator() override;

  // Registers lens entry in the side panel and shows side panel with lens
  // selected if its not shown
  void RegisterEntryAndShow(const content::OpenURLParams& params);

  content::WebContents* GetViewWebContentsForTesting();

  bool OpenResultsInNewTabForTesting();

  bool IsLaunchButtonEnabledForTesting();

 private:
  friend class BrowserUserData<LensSidePanelCoordinator>;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  bool IsDefaultSearchProviderGoogle();

  std::u16string GetComboboxLabel();

  const gfx::VectorIcon& GetComboboxIcon();

  BrowserView* GetBrowserView();

  // Removes the lens entry from the side panel.
  void DeregisterLensFromSidePanel();

  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override;

  // SidePanelViewStateObserver
  void OnSidePanelDidClose() override;

  std::unique_ptr<views::View> CreateLensWebView(
      const content::OpenURLParams& params);

  raw_ptr<TemplateURLService> template_url_service_;
  base::WeakPtr<lens::LensUnifiedSidePanelView> lens_side_panel_view_;
  raw_ptr<const TemplateURL> current_default_search_provider_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_SIDE_PANEL_COORDINATOR_H_
