// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#include "chrome/browser/ui/lens/lens_side_panel_navigation_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/web_applications/web_app_icon_downloader.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/favicon_base/favicon_util.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/lens/lens_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {
std::unique_ptr<views::WebView> CreateWebView(
    views::View* host,
    content::BrowserContext* browser_context) {
  auto web_view = std::make_unique<views::WebView>(browser_context);
  // Set a flex behavior for the WebView to always fill out the extra space in
  // the parent view. In the minimum case, it will scale down to 0.
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // Set background of webview to the same background as the toolbar. This is to
  // prevent personal color themes from showing in the side panel when
  // navigating to a new Lens results panel.
  web_view->SetBackground(views::CreateThemedSolidBackground(kColorToolbar));
  return web_view;
}

}  // namespace

namespace lens {
constexpr char kStaticLoadingScreenURL[] =
    "https://www.gstatic.com/lens/chrome/lens_side_panel_loading.html";

LensUnifiedSidePanelView::LensUnifiedSidePanelView(
    BrowserView* browser_view,
    base::RepeatingCallback<void()> update_new_tab_button_callback)
    : browser_view_(browser_view),
      update_new_tab_button_callback_(update_new_tab_button_callback) {
  auto* browser_context = browser_view->GetProfile();
  // Align views vertically top to bottom.
  SetOrientation(views::LayoutOrientation::kVertical);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);

  // Allow view to be focusable in order to receive focus when side panel is
  // opened.
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Stretch views to fill horizontal bounds.
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  loading_indicator_web_view_ =
      AddChildView(CreateWebView(this, browser_context));
  loading_indicator_web_view_->GetWebContents()->GetController().LoadURL(
      GURL(kStaticLoadingScreenURL), content::Referrer(),
      ui::PAGE_TRANSITION_FROM_API, std::string());
  web_view_ = AddChildView(CreateWebView(this, browser_context));
  separator_ = AddChildView(std::make_unique<views::Separator>());

  SetContentAndNewTabButtonVisible(/* visible= */ false,
                                   /* enable_new_tab_button= */ false);

  auto* web_contents = web_view_->GetWebContents();
  web_contents->SetDelegate(this);
  Observe(web_contents);

  auto* profile = browser_view->GetProfile();
  download::DownloadNavigationObserver::CreateForWebContents(
      web_contents,
      download::NavigationMonitorFactory::GetForKey(profile->GetProfileKey()));

  // Setup NavigationThrottler to stop navigation outside of current domain
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* const provider = service->GetDefaultSearchProvider();
  lens::LensSidePanelNavigationHelper::CreateForWebContents(
      web_contents, browser_view->browser(),
      search::DefaultSearchProviderIsGoogle(profile)
          ? lens::features::GetHomepageURLForLens()
          : provider->image_url());
}

content::WebContents* LensUnifiedSidePanelView::GetWebContents() {
  return web_view_->GetWebContents();
}

TemplateURLService* LensUnifiedSidePanelView::GetTemplateURLService() {
  auto* web_contents = web_view_->GetWebContents();
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);
  return template_url_service;
}

bool LensUnifiedSidePanelView::IsDefaultSearchProviderGoogle() {
  auto* web_contents = web_view_->GetWebContents();
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return search::DefaultSearchProviderIsGoogle(profile);
}

GURL LensUnifiedSidePanelView::GetOpenInNewTabURL() {
  const GURL last_committed_url =
      web_view_->GetWebContents()->GetLastCommittedURL();
  const GURL url = IsDefaultSearchProviderGoogle()
                       ? lens::CreateURLForNewTab(last_committed_url)
                       : last_committed_url;
  // If there is no payload parameter, we will have an empty URL. This means
  // we should return on empty and not close the side panel.
  return url.is_empty()
             ? GURL()
             : GetTemplateURLService()->RemoveSideImageSearchParamFromURL(url);
}

void LensUnifiedSidePanelView::LoadResultsInNewTab() {
  const GURL last_committed_url =
      web_view_->GetWebContents()->GetLastCommittedURL();
  const GURL url = IsDefaultSearchProviderGoogle()
                       ? lens::CreateURLForNewTab(last_committed_url)
                       : last_committed_url;
  // If there is no payload parameter, we will have an empty URL. This means
  // we should return on empty and not close the side panel.
  if (url.is_empty())
    return;
  const GURL modified_url =
      GetTemplateURLService()->RemoveSideImageSearchParamFromURL(url);
  content::OpenURLParams params(modified_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_TYPED,
                                /*is_renderer_initiated=*/false);
  browser_view_->browser()->OpenURL(params);
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.LoadResultsInNewTab"));
  browser_view_->side_panel_coordinator()->Close();
}

void LensUnifiedSidePanelView::DocumentOnLoadCompletedInPrimaryMainFrame() {
  auto last_committed_url = web_view_->GetWebContents()->GetLastCommittedURL();

  if (!IsDefaultSearchProviderGoogle()) {
    SetContentAndNewTabButtonVisible(/* visible= */ true,
                                     /* enable_new_tab_button= */ true);
    return;
  }

  // Since Lens Web redirects to the actual UI using HTML redirection, this
  // method gets fired twice. This check ensures we only show the user the
  // rendered page and not the redirect. It also ensures we immediately render
  // any page that is not lens.google.com
  // TODO(243935799): Cleanup this check once Lens Web no longer redirects
  if (lens::ShouldPageBeVisible(last_committed_url))
    SetContentAndNewTabButtonVisible(
        /* visible= */ true,
        /* enable_new_tab_button= */ lens::IsValidLensResultUrl(
            last_committed_url));
}

// Catches case where Chrome errors. I.e. no internet connection
// TODO(243935799): Cleanup this listener once Lens Web no longer redirects
void LensUnifiedSidePanelView::PrimaryPageChanged(content::Page& page) {
  auto last_committed_url = web_view_->GetWebContents()->GetLastCommittedURL();

  if (page.GetMainDocument().IsErrorDocument()) {
    bool enable_new_tab_button =
        IsDefaultSearchProviderGoogle()
            ? lens::IsValidLensResultUrl(last_committed_url)
            : true;
    SetContentAndNewTabButtonVisible(/* visible= */ true,
                                     enable_new_tab_button);
  }
}

void LensUnifiedSidePanelView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  return web_view_->GetAccessibleNodeData(node_data);
}

bool LensUnifiedSidePanelView::IsLaunchButtonEnabledForTesting() {
  return !update_new_tab_button_callback_.is_null();
}

content::WebContents* LensUnifiedSidePanelView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (lens::features::GetEnableContextMenuInLensSidePanel()) {
    // Use |OpenURL| so that we create a new tab rather than trying to open
    // this link in the side panel.
    return browser_view_->browser()->OpenURL(params);
  } else {
    return content::WebContentsDelegate::OpenURLFromTab(source, params);
  }
}

void LensUnifiedSidePanelView::OpenUrl(const content::OpenURLParams& params) {
  side_panel_url_params_ = std::make_unique<content::OpenURLParams>(params);
  SetContentAndNewTabButtonVisible(/* visible= */ false,
                                   /* enable_new_tab_button= */ false);
  MaybeLoadURLWithParams();
}

void LensUnifiedSidePanelView::DidOpenRequestedURL(
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
  if (lens::features::GetEnableContextMenuInLensSidePanel() &&
      started_from_context_menu) {
    // Link clicks initiated from the context menu will be handled by
    // |OpenURLFromTab|.
    return;
  }

  // If the navigation is initiated by the renderer process, we must set an
  // initiator origin.
  if (renderer_initiated)
    params.initiator_origin = url::Origin::Create(url);

  browser_view_->browser()->OpenURL(params);
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.ResultLinkClick"));
}

void LensUnifiedSidePanelView::MaybeLoadURLWithParams() {
  // Ensure the side panel view has a width before loading URL. If side panel is
  // still closed (width == 0), defer loading the URL to
  // LensUnifiedSidePanelView::OnViewBoundsChanged. The nullptr check ensures we
  // don't rerender the same page on a unrelated resize event.
  if (width() == 0 || !side_panel_url_params_)
    return;

  // Manually set web contents to the size of side panel view on initial load.
  // This prevents a bug in Lens Web that renders the page as if it was 0px
  // wide.
  GetWebContents()->Resize(bounds());
  GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(*side_panel_url_params_));
  side_panel_url_params_.reset();
}

void LensUnifiedSidePanelView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // If side panel is closed when we first try to render the URL, we must wait
  // until side panel is opened. This method is called once side panel view goes
  // from 0px wide to ~320px wide. Rendering the page after side panel view
  // fully opens prevents a race condition which causes the page to load before
  // side panel is open causing the page to render as if it were 0px wide.
  MaybeLoadURLWithParams();
}

void LensUnifiedSidePanelView::SetContentAndNewTabButtonVisible(
    bool visible,
    bool enable_new_tab_button) {
  web_view_->SetVisible(visible);
  loading_indicator_web_view_->SetVisible(!visible);

  if (!update_new_tab_button_callback_.is_null())
    update_new_tab_button_callback_.Run();
}

LensUnifiedSidePanelView::~LensUnifiedSidePanelView() = default;

}  // namespace lens
