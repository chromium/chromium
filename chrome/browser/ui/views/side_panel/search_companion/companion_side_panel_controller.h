// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "components/lens/buildflags.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace companion {

// Controller for handling views specific logic for the CompanionTabHelper.
class CompanionSidePanelController : public CompanionTabHelper::Delegate,
                                     public content::WebContentsObserver,
                                     public SidePanelEntryObserver {
 public:
  explicit CompanionSidePanelController(content::WebContents* web_contents);
  CompanionSidePanelController(const CompanionSidePanelController&) = delete;
  CompanionSidePanelController& operator=(const CompanionSidePanelController&) =
      delete;
  ~CompanionSidePanelController() override;

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // CompanionTabHelper::Delegate:
  void AddCompanionFinishedLoadingCallback(
      CompanionTabHelper::CompanionLoadedCallback callback) override;
  void CreateAndRegisterEntry() override;
  void DeregisterEntry() override;
  void ShowCompanionSidePanel(
      SidePanelOpenTrigger side_panel_open_trigger) override;
  void UpdateNewTabButton(GURL url_to_open) override;
  void OnCompanionSidePanelClosed() override;
  bool IsCompanionShowing() override;
  void SetCompanionAsActiveEntry(content::WebContents* contents) override;
  void OpenContextualLensView(const content::OpenURLParams& params) override;
  content::WebContents* GetLensViewWebContentsForTesting() override;
  bool OpenLensResultsInNewTabForTesting() override;
  bool IsLensLaunchButtonEnabledForTesting() override;

  content::WebContents* GetCompanionWebContentsForTesting() override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  std::unique_ptr<views::View> CreateCompanionWebView();
  std::unique_ptr<views::View> CreateContextualLensView(
      const content::OpenURLParams& params);
  GURL GetOpenInNewTabUrl();
  GURL GetLensOpenInNewTabButtonURL();

  // Method used as a callback to notify the Search Companion server of a link
  // click once communication with the page has been initialized.
  void NotifyLinkClick(GURL opened_url,
                       side_panel::mojom::LinkOpenMetadataPtr metadata,
                       content::WebContents* main_tab_contents);

  // Returns true if the `url` matches the one used by the Search Companion
  // website.
  bool IsSiteTrusted(const GURL& url);

  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;

  void AddObserver();
  void RemoveObserver();

  void UpdateNewTabButtonState();

  GURL open_in_new_tab_url_;
  std::vector<CompanionTabHelper::CompanionLoadedCallback>
      companion_loaded_callbacks_;
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  base::WeakPtr<lens::LensUnifiedSidePanelView> lens_side_panel_view_;
#endif
  // Container view so we can easily swap views under the same entry when Lens
  // is contextual.
  raw_ptr<views::View> panel_container_view_;
  // Pointer to future content we want to render on the companion panel the next
  // time it opens.
  std::unique_ptr<views::View> future_content_view_;
  const raw_ptr<content::WebContents> web_contents_;
  bool has_companion_loaded_ = false;
  bool is_lens_view_showing_ = false;

  base::WeakPtrFactory<CompanionSidePanelController> weak_ptr_factory_{this};
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SEARCH_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_H_
