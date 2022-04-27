// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_entrypoints.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/webview/webview.h"

namespace {

GURL CreateURLForNewTab(const GURL& original_url) {
  // We need to create a new URL with the specified |query_parameters| while
  // also keeping the payloard parameter in the original URL.
  if (original_url.is_empty())
    return GURL();

  std::string payload;
  // Make sure the payload is present.
  if (!net::GetValueForKeyInQuery(original_url, lens::kPayloadQueryParameter,
                                  &payload))
    return GURL();

  GURL modified_url;
  // Append or replace query parameters related to entry point.
  modified_url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens::EntryPoint::CHROME_OPEN_NEW_TAB_SIDE_PANEL,
      /*use_side_panel=*/false);
  return modified_url;
}

}  // namespace

namespace lens {

LensSidePanelController::LensSidePanelController(
    base::OnceClosure close_callback,
    SidePanel* side_panel,
    BrowserView* browser_view)
    : close_callback_(std::move(close_callback)),
      side_panel_(side_panel),
      browser_view_(browser_view),
      side_panel_view_(
          side_panel_->AddChildView(std::make_unique<lens::LensSidePanelView>(
              browser_view_->GetProfile(),
              base::BindRepeating(&LensSidePanelController::CloseButtonClicked,
                                  base::Unretained(this)),
              base::BindRepeating(&LensSidePanelController::LoadResultsInNewTab,
                                  base::Unretained(this))))) {
  side_panel_->SetVisible(false);
  Observe(side_panel_view_->GetWebContents());
  side_panel_view_->GetWebContents()->SetDelegate(this);
}

LensSidePanelController::~LensSidePanelController() = default;

void LensSidePanelController::OpenWithURL(
    const content::OpenURLParams& params) {
  if (browser_view_->CloseOpenRightAlignedSidePanel(
          /*exclude_lens=*/true,
          /*exclude_side_search=*/false)) {
    base::RecordAction(
        base::UserMetricsAction("LensSidePanel.HideChromeSidePanel"));
  }

  browser_view_->MaybeClobberAllSideSearchSidePanels();

  side_panel_view_->GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
  if (side_panel_->GetVisible()) {
    // The user issued a follow-up Lens query.
    base::RecordAction(
        base::UserMetricsAction("LensSidePanel.LensQueryWhileShowing"));
  } else {
    side_panel_->SetVisible(true);
    base::RecordAction(base::UserMetricsAction("LensSidePanel.Show"));
  }
}

bool LensSidePanelController::IsShowing() const {
  return side_panel_->GetVisible();
}

void LensSidePanelController::Close() {
  if (side_panel_->GetVisible()) {
    // Loading an empty URL on close prevents old results from being displayed
    // in the side panel if the side panel is reopened.
    side_panel_view_->GetWebContents()->GetController().LoadURL(
        GURL(), content::Referrer(), ui::PAGE_TRANSITION_FROM_API,
        std::string());
    side_panel_->SetVisible(false);
    browser_view_->RightAlignedSidePanelWasClosed();
    base::RecordAction(base::UserMetricsAction("LensSidePanel.Hide"));
  }
  std::move(close_callback_).Run();
}

void LensSidePanelController::LoadResultsInNewTab() {
  if (side_panel_view_ && side_panel_view_->GetWebContents()) {
    // Open the latest URL visible on the side panel. This accounts for when the
    // user uploads an image to Lens via drag and drop. This also allows any
    // region selection changes to transfer to the new tab.
    GURL url = CreateURLForNewTab(
        side_panel_view_->GetWebContents()->GetLastCommittedURL());
    // If there is no payload parameter, we will have an empty URL. This means
    // we should return on empty and not close the side panel.
    if (url.is_empty())
      return;
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_TYPED,
                                  /*is_renderer_initiated=*/false);
    browser_view_->browser()->OpenURL(params);
    base::RecordAction(
        base::UserMetricsAction("LensSidePanel.LoadResultsInNewTab"));
  }
  Close();
}

bool LensSidePanelController::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
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
  content::OpenURLParams params(url, referrer, disposition, transition,
                                renderer_initiated);
  // If the navigation is initiated by the renderer process, we must set an
  // initiator origin.
  if (renderer_initiated)
    params.initiator_origin = url::Origin::Create(url);
  browser_view_->browser()->OpenURL(params);
  base::RecordAction(base::UserMetricsAction("LensSidePanel.ResultLinkClick"));
}

void LensSidePanelController::CloseButtonClicked() {
  base::RecordAction(base::UserMetricsAction("LensSidePanel.CloseButtonClick"));
  Close();
}

void LensSidePanelController::LoadProgressChanged(double progress) {
  if(progress == 1.0) {
    side_panel_view_->SetContentVisible(true);
  } else {
    side_panel_view_->SetContentVisible(false);
  }
}

}  // namespace lens
