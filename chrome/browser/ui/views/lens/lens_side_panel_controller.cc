// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"

#include "base/bind.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/lens/lens_entrypoints.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/views/controls/webview/webview.h"

namespace {

// TODO(crbug/1250542): Refactor this to create OpenURLParams in core_tab_helper
// instead of replacing query params with GURL API.
std::unique_ptr<content::OpenURLParams> CreateOpenNewTabURLParamsForNewTab(
    const content::OpenURLParams& params) {
  // We want to modify the original GURL to have query parameters pertaining to
  // this entry point.
  content::OpenURLParams new_tab_params(params);
  GURL original_url = params.url;
  GURL::Replacements replacements;
  std::string new_query = lens::GetQueryParametersForLensRequest(
      lens::EntryPoint::CHROME_OPEN_NEW_TAB_SIDE_PANEL,
      /*use_side_panel=*/false);
  replacements.SetQuery(new_query.c_str(),
                        url::Component(0, new_query.length()));
  new_tab_params.url = original_url.ReplaceComponents(replacements);
  return std::make_unique<content::OpenURLParams>(new_tab_params);
}

}  // namespace

namespace lens {

LensSidePanelController::LensSidePanelController(SidePanel* side_panel,
                                                 BrowserView* browser_view)
    : lens_web_params_(nullptr),
      side_panel_(side_panel),
      browser_view_(browser_view),
      side_panel_view_(
          side_panel_->AddChildView(std::make_unique<lens::LensSidePanelView>(
              browser_view_->GetProfile(),
              base::BindRepeating(&LensSidePanelController::Close,
                                  base::Unretained(this)),
              base::BindRepeating(&LensSidePanelController::LoadResultsInNewTab,
                                  base::Unretained(this))))) {
  side_panel_->SetVisible(false);
  Observe(side_panel_view_->GetWebContents());
}

LensSidePanelController::~LensSidePanelController() = default;

void LensSidePanelController::OpenWithURL(
    const content::OpenURLParams& params) {
  // Check if read later is enabled, before hiding side panel.
  if (browser_view_->toolbar()->read_later_button())
    browser_view_->toolbar()->read_later_button()->HideSidePanel();
  side_panel_view_->GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  lens_web_params_ = CreateOpenNewTabURLParamsForNewTab(params);
  side_panel_->SetVisible(true);
}

void LensSidePanelController::Close() {
  lens_web_params_ = nullptr;
  side_panel_->SetVisible(false);
}

void LensSidePanelController::LoadResultsInNewTab() {
  if (lens_web_params_) {
    browser_view_->browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->OpenURL(*lens_web_params_);
  }
  Close();
}

void LensSidePanelController::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  browser_view_->browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .LoadURLWithParams(content::NavigationController::LoadURLParams(url));
}

}  // namespace lens
