// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_UNIFIED_SIDE_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_UNIFIED_SIDE_PANEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/views/layout/flex_layout_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Separator;
class WebView;
}  // namespace views

namespace lens {

// Owns the lens webview and navigates to a google lens URL when requested. Its
// owned by the side panel registry.
class LensUnifiedSidePanelView : public views::FlexLayoutView,
                                 public content::WebContentsObserver,
                                 public content::WebContentsDelegate {
 public:
  LensUnifiedSidePanelView(
      BrowserView* browser_view,
      base::RepeatingClosure update_new_tab_button_callback);
  LensUnifiedSidePanelView(const LensUnifiedSidePanelView&) = delete;
  LensUnifiedSidePanelView& operator=(const LensUnifiedSidePanelView&) = delete;
  ~LensUnifiedSidePanelView() override;

  // views::View
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  content::WebContents* GetWebContents();

  void OpenUrl(const content::OpenURLParams& params);

  // Loads the Lens website if the side panel view is ready with a width.
  void MaybeLoadURLWithParams();

  // Gets the URL needed to open the results shown in the view in a new tab.
  GURL GetOpenInNewTabURL();

  // Opens current view URL in a new chrome tab and closes the side panel
  void LoadResultsInNewTab();

  base::WeakPtr<LensUnifiedSidePanelView> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  bool IsLaunchButtonEnabledForTesting();

 private:
  TemplateURLService* GetTemplateURLService();

  bool IsDefaultSearchProviderGoogle();

  // Shows / hides the lens results and the loading view to avoid showing
  // loading artifacts. If the visible bool is false, show loading view else
  // show lens results view. Also enables/disables the new tab button depending
  // if the lens results page is showing.
  void SetContentAndNewTabButtonVisible(bool visible,
                                        bool enable_new_tab_button);
  // Sets the content and new tab button visibility for the given URL.
  // The contents will be made visible if the URL is a valid Lens results URL,
  // an error page, or a non-Lens URL. The new tab button will be made visible
  // only if the URL is a valid Lens results URL.
  void MaybeSetContentAndNewTabButtonVisible(const GURL& url);

  // Registers a WebContentsModalDialogManager for our WebContents in order to
  // display web modal dialogs triggered by it.
  void RegisterModalDialogManager(Browser* browser);

  // content::WebContentsObserver:
  // TODO(crbug.com/40916154): Clean up unused listeners and flags after
  // determining which ones we want to listen to for server-side rendering
  // backends.
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void PrimaryPageChanged(content::Page& page) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;

  raw_ptr<BrowserView> browser_view_;
  raw_ptr<views::Separator> separator_;
  raw_ptr<views::WebView> loading_indicator_web_view_;
  raw_ptr<views::WebView> web_view_;

  // Copy of the most recent URL params to open.
  std::unique_ptr<content::OpenURLParams> side_panel_url_params_;

  base::RepeatingClosure update_new_tab_button_callback_;
  base::WeakPtrFactory<LensUnifiedSidePanelView> weak_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_UNIFIED_SIDE_PANEL_VIEW_H_
