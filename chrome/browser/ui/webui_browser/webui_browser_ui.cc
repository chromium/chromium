// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/webui_browser_resources.h"
#include "chrome/grit/webui_browser_resources_map.h"
#include "components/guest_contents/browser/guest_contents_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

WebUIBrowserUIConfig::WebUIBrowserUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIWebuiBrowserHost) {}

bool WebUIBrowserUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return webui_browser::IsWebUIBrowserEnabled();
}

WebUIBrowserUI::WebUIBrowserUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
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

  // As a demonstration of passing a variable for JS to use we pass in some
  // a simple message.
  source->AddString("message", "Hello World from C++!");

  // Make a guest contents handle for a test guest contents.
  // TODO(webium): Remove once the tab strip is integrated.
  test_guest_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(browser_->profile()));
  test_guest_contents_->GetController().LoadURL(
      GURL("chrome://dino"), content::Referrer(), ui::PAGE_TRANSITION_FIRST,
      std::string());
  guest_contents::GuestId guest_id =
      guest_contents::GuestContentsHandle::CreateForWebContents(
          test_guest_contents_.get())
          ->id();
  source->AddInteger("testGuestId", guest_id);
}

WebUIBrowserUI::~WebUIBrowserUI() = default;

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<webui_browser::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver) {
  guest_contents::GuestContentsHostImpl::Create(web_ui()->GetWebContents(),
                                                std::move(receiver));
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

void WebUIBrowserUI::ShowSidePanel(SidePanelEntryKey side_panel_entry_key) {
  // TODO(webium): Create side panel and call page_->ShowSidePanel()
  NOTIMPLEMENTED();
}

void WebUIBrowserUI::CloseSidePanel() {
  // TODO(webium): Create side panel and call page_->CloseSidePanel()
  NOTIMPLEMENTED();
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIBrowserUI)
