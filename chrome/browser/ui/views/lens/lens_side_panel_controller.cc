// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_entrypoints.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

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
                                  base::Unretained(this))))),
      side_panel_url_params_(nullptr) {
  side_panel_->SetVisible(false);
  Observe(side_panel_view_->GetWebContents());
  side_panel_view_->GetWebContents()->SetDelegate(this);

  // Observe changes in the side_panel_view_ sizing.
  side_panel_view_->AddObserver(this);
}

LensSidePanelController::~LensSidePanelController() {
  side_panel_view_->RemoveObserver(this);

  // check side_panel -> children() size for unit tests where all the children
  // are removed when side panel is destroyed.
  if (side_panel_view_ != nullptr && side_panel_->children().size() != 0) {
    // Destroy the side panel view added in the constructor. side_panel_ has the
    // browser_view life span but controller gets created and destroyed each
    // time the side panel is opened and closed.
    std::unique_ptr<lens::LensSidePanelView> side_panel_view_unique_ptr =
        side_panel_->RemoveChildViewT(side_panel_view_);
    if (side_panel_view_unique_ptr != nullptr) {
      side_panel_view_unique_ptr.reset();
    }
  }
}

void LensSidePanelController::OpenWithURL(
    const content::OpenURLParams& params) {
  if (browser_view_->CloseOpenRightAlignedSidePanel(
          /*exclude_lens=*/true,
          /*exclude_side_search=*/false)) {
    base::RecordAction(
        base::UserMetricsAction("LensSidePanel.HideChromeSidePanel"));
  }

  browser_view_->MaybeClobberAllSideSearchSidePanels();

  if (side_panel_->GetVisible()) {
    // The user issued a follow-up Lens query.
    base::RecordAction(
        base::UserMetricsAction("LensSidePanel.LensQueryWhileShowing"));
  } else {
    side_panel_->SetVisible(true);
    base::RecordAction(base::UserMetricsAction("LensSidePanel.Show"));
  }

  side_panel_url_params_ = std::make_unique<content::OpenURLParams>(params);
  side_panel_view_->SetContentAndNewTabButtonVisible(false, false);
  MaybeLoadURLWithParams();
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
    GURL url = lens::CreateURLForNewTab(
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

void LensSidePanelController::MaybeLoadURLWithParams() {
  // Ensure side panel has a width before loading URL. If side panel is still
  // closed (width == 0), defer loading the URL to
  // LensSidePanelController::OnViewBoundsChanged. The nullptr check ensures we
  // don't rerender the same page on a unrelated resize event.
  if (side_panel_view_->width() == 0 || !side_panel_url_params_)
    return;
  // Manually set web contents to the size of side panel view on initial load.
  // This prevents a bug in Lens Web that renders the page as if it was 0px
  // wide.
  auto* web_contents = side_panel_view_->GetWebContents();
  web_contents->Resize(side_panel_view_->bounds());
  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(*side_panel_url_params_));

  side_panel_url_params_.reset();
}

void LensSidePanelController::OnViewBoundsChanged(views::View* observed_view) {
  // If side panel is closed when we first try to render the URL, we must wait
  // until side panel is opened. This method is called once side panel view goes
  // from 0px wide to ~320px wide. Rendering the page after it fully opens
  // prevents a race condition which causes the page to load before side panel
  // is open causing the page to render as if it were 0px wide.
  MaybeLoadURLWithParams();
}

void LensSidePanelController::DocumentOnLoadCompletedInPrimaryMainFrame() {
  auto last_committed_url =
      side_panel_view_->GetWebContents()->GetLastCommittedURL();

  // Since Lens Web redirects to the actual UI using HTML redirection, this
  // method gets fired twice. This check ensures we only show the user the
  // rendered page and not the redirect. It also ensures we immediately render
  // any page that is not lens.google.com
  // TODO(243935799): Cleanup this check once Lens Web no longer redirects
  if (lens::ShouldPageBeVisible(last_committed_url))
    side_panel_view_->SetContentAndNewTabButtonVisible(
        true, lens::IsValidLensResultUrl(last_committed_url));
}

// Catches case where Chrome errors. I.e. no internet connection
// TODO(243935799): Cleanup this listener once Lens Web no longer redirects
void LensSidePanelController::PrimaryPageChanged(content::Page& page) {
  auto last_committed_url =
      side_panel_view_->GetWebContents()->GetLastCommittedURL();

  if (page.GetMainDocument().IsErrorDocument())
    side_panel_view_->SetContentAndNewTabButtonVisible(
        true, lens::IsValidLensResultUrl(last_committed_url));
}

}  // namespace lens
