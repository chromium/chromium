// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/web_view_side_panel_view.h"

#include <string_view>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/page_info/web_view_side_panel_throttle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "url/gurl.h"
#include "url/origin.h"

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
  // navigating to a new results panel.
  web_view->SetBackground(views::CreateSolidBackground(kColorToolbar));
  return web_view;
}
}  // namespace

WebViewSidePanelView::WebViewSidePanelView(
    content::WebContents* parent_web_contents,
    const std::string& loading_screen_url,
    const std::optional<std::string>& param_name_to_cleanup)
    : parent_web_contents_(parent_web_contents->GetWeakPtr()),
      param_name_to_cleanup_(param_name_to_cleanup) {
  auto* browser_context = outer_browser_view()->GetProfile();

  // Allow view to be focusable in order to receive focus when side panel is
  // opened.
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Align views vertically top to bottom.
  SetOrientation(views::LayoutOrientation::kVertical);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);

  // Stretch views to fill horizontal bounds.
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  loading_indicator_web_view_ =
      AddChildView(CreateWebView(this, browser_context));
  loading_indicator_web_view_->GetWebContents()->GetController().LoadURL(
      GURL(loading_screen_url), content::Referrer(),
      ui::PAGE_TRANSITION_FROM_API, std::string());
  web_view_ = AddChildView(CreateWebView(this, browser_context));

  SetContentVisible(false);
  auto* web_contents = web_view_->GetWebContents();
  web_contents->SetDelegate(this);
  web_contents->SetUserData(
      kWebViewSidePanelWebContentsUserDataKey,
      std::make_unique<WebViewSidePanelWebContentsUserData>(AsWeakPtr()));
  Observe(web_contents);

  GetViewAccessibility().SetRole(ax::mojom::Role::kWebView);
  GetViewAccessibility().SetName(
      std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

void WebViewSidePanelView::LoadProgressChanged(double progress) {
  // Ignore the initial load progress since the navigation might be intercepted
  // by WebViewSidePanelThrottle.
  if (progress == blink::kInitialLoadProgress) {
    return;
  }
  SetContentVisible(progress == blink::kFinalLoadProgress);
}

void WebViewSidePanelView::OpenUrl(const content::OpenURLParams& params) {
  last_url_ = params.url;
  web_view_->GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
}

GURL WebViewSidePanelView::GetLastUrlForTesting() {
 return last_url_;
}

// This method is called when the WebContents wants to open a link in a new
// tab. This delegate does not override AddNewContents(), so the webcontents
// is not actually created. Instead it forwards the parameters to the real
// browser.
void WebViewSidePanelView::DidOpenRequestedURL(
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
  if (renderer_initiated) {
    params.initiator_origin = url::Origin::Create(url);
  }

  // We can't open a new tab while the observer is running because it might
  // destroy this WebContents. Post as task instead.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebViewSidePanelView::OpenUrlInBrowser,
                                AsWeakPtr(), std::move(params)));
}

// This function is called when the user opens a link in a new tab or window
// e.g. from the context menu or by middle-clicking on it. It forwards the
// params to the main browser and does not create a WebContents in the
// SidePanel.
content::WebContents* WebViewSidePanelView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // Redirect requests to open a new tab to the main browser. These come e.g.
  // from the context menu.
  content::OpenURLParams new_params(params);
  new_params.url = CleanUpQueryParams(params.url);
  if (auto* delegate = outer_delegate()) {
    delegate->OpenURLFromTab(source, new_params,
                             std::move(navigation_handle_callback));
  }
  return nullptr;
}

bool WebViewSidePanelView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Redirect keyboard events to the main browser.
  if (auto* delegate = outer_delegate()) {
    return delegate->HandleKeyboardEvent(source, event);
  }
  return false;
}

BrowserView* WebViewSidePanelView::outer_browser_view() {
  if (parent_web_contents_) {
    auto* browser = chrome::FindBrowserWithTab(parent_web_contents_.get());
    return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
  }
  return nullptr;
}

content::WebContentsDelegate* WebViewSidePanelView::outer_delegate() {
  auto* browser_view = outer_browser_view();
  return browser_view ? browser_view->browser() : nullptr;
}

void WebViewSidePanelView::OpenUrlInBrowser(
    const content::OpenURLParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* browser_view = outer_browser_view()) {
    content::OpenURLParams new_params(params);
    new_params.url = CleanUpQueryParams(params.url);
    browser_view->browser()->OpenURL(new_params,
                                     /*navigation_handle_callback=*/{});
  }
}

GURL WebViewSidePanelView::CleanUpQueryParams(const GURL& url) {
  // Override eventual parameter for navigations to a real tab.
  if (url::IsSameOriginWith(url, last_url_) &&
      param_name_to_cleanup_.has_value() &&
      url.query().find(param_name_to_cleanup_.value()) !=
          std::string_view::npos) {
    return net::AppendOrReplaceQueryParameter(
        url, param_name_to_cleanup_.value(), std::string());
  }
  return url;
}

void WebViewSidePanelView::SetContentVisible(bool visible) {
  web_view_->SetVisible(visible);
  loading_indicator_web_view_->SetVisible(!visible);
}

WebViewSidePanelView::~WebViewSidePanelView() = default;
