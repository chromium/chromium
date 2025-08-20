// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_register.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui_browser/bookmark_bar_page_handler.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/tab_strip_api_resources_map.h"
#include "chrome/grit/webui_browser_resources.h"
#include "chrome/grit/webui_browser_resources_map.h"
#include "components/guest_contents/browser/guest_contents_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/webui_util.h"

WebUIBrowserUIConfig::WebUIBrowserUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIWebuiBrowserHost) {}

bool WebUIBrowserUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return webui_browser::IsWebUIBrowserEnabled();
}

// realbox uses ui/webui/resources/js/metrics_reporter, which in turn uses
// chrome.timeTicks.nowInMicroseconds(). In order to provide it we pass
// enable_chrome_send to MojoWebUIController constructor, since they're
// a package deal via BindingsPolicyValue::kWebUi.
WebUIBrowserUI::WebUIBrowserUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  WebUIBrowserWindow* webui_browser_window =
      WebUIBrowserWindow::FromWebShellWebContents(web_ui->GetWebContents());
  browser_ = webui_browser_window->browser();

  // Set up the chrome://webui-browser source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWebuiBrowserHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kWebuiBrowserResources,
                              IDR_WEBUI_BROWSER_WEBUI_BROWSER_HTML);
  source->AddResourcePaths(kTabStripApiResources);

  SearchboxHandler::SetupWebUIDataSource(source, Profile::FromWebUI(web_ui));
}

WebUIBrowserUI::~WebUIBrowserUI() = default;

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<webui_browser::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<bookmark_bar::mojom::PageHandlerFactory> receiver) {
  bookmark_bar_page_factory_receiver_.reset();
  bookmark_bar_page_factory_receiver_.Bind(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  content::WebUI* webui = web_ui();
  content::WebContents* web_contents = webui->GetWebContents();
  realbox_handler_ = std::make_unique<RealboxHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(webui), web_contents,
      &metrics_reporter_, /*omnibox_controller=*/nullptr);
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  metrics_reporter_.BindInterface(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver) {
  guest_contents::GuestContentsHostImpl::Create(web_ui()->GetWebContents(),
                                                std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver) {
  auto* tab_strip_service =
      browser_->browser_window_features()->tab_strip_service();
  CHECK(tab_strip_service) << "Browser missing TabStripService, did you enable "
                              "TabStripBrowserApi feature flag?";
  tab_strip_service->Accept(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        receiver) {
  // Make sure BrowserView and WebUIBrowserWindow use the same context.
  // In this case, they both use the container widget as context.
  // This allows the following code to work in both Webium and BrowserView.
  //
  //   ui::ElementContext context(BrowserElements::From(browser)->GetContext());
  //   ui::TrackedElement* element = ui::ElementTracker::GetElementTracker()
  //       ->GetFirstMatchingElement(kToolbarAppMenuButtonElementId, context);
  //   const gfx::Rect rect = element->GetVisibleBoundsInScreen();
  //   ...
  //
  // TODO(webium): use BrowserElements::From(browser)->GetContext(). This
  // requires adding a BrowserElementsWebUI.
  const ui::ElementContext context =
      ui::ElementContext(browser_window()->widget());
  tracked_element_handler_ = std::make_unique<ui::TrackedElementHandler>(
      web_ui()->GetWebContents(), std::move(receiver), context,
      GetKnownElementIdentifiers());
}

base::WeakPtr<WebUIBrowserUI> WebUIBrowserUI::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebUIBrowserUI::CreatePageHandler(
    mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver) {
  auto* render_frame_host = web_ui()->GetRenderFrameHost();
  WebUIBrowserPageHandler::CreateForRenderFrameHost(*render_frame_host,
                                                    std::move(receiver), this);
}

void WebUIBrowserUI::CreatePageHandler(
    mojo::PendingRemote<bookmark_bar::mojom::Page> page,
    mojo::PendingReceiver<bookmark_bar::mojom::PageHandler> receiver) {
  bookmark_bar_page_handler_ =
      std::make_unique<WebUIBrowserBookmarkBarPageHandler>(
          std::move(receiver), std::move(page), web_ui(), browser_);
}

const std::vector<ui::ElementIdentifier>&
WebUIBrowserUI::GetKnownElementIdentifiers() const {
  static const std::vector<ui::ElementIdentifier> kKnownElementIdentifiers{
      kToolbarAppMenuButtonElementId};
  return kKnownElementIdentifiers;
}

void WebUIBrowserUI::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  if (bookmark_bar_page_handler_) {
    bookmark_bar_page_handler_->SetBookmarkBarState(
        BookmarkBarController::From(browser_)->bookmark_bar_state(),
        change_type);
  }
}

void WebUIBrowserUI::ShowSidePanel(SidePanelEntryKey side_panel_entry_key) {
  // TODO(webium): Create side panel and call page_->ShowSidePanel()
  NOTIMPLEMENTED();
}

void WebUIBrowserUI::CloseSidePanel() {
  // TODO(webium): Create side panel and call page_->CloseSidePanel()
  NOTIMPLEMENTED();
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIBrowserUI)
