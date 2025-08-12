// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_

#include <memory>

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui_browser/browser.mojom.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Browser;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace searchbox::mojom {
class PageHandler;
}  // namespace searchbox::mojom

class RealboxHandler;
class WebUIBrowserUI;

class WebUIBrowserUIConfig
    : public content::DefaultWebUIConfig<WebUIBrowserUI> {
 public:
  WebUIBrowserUIConfig();

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://webui-browser
class WebUIBrowserUI : public ui::MojoWebUIController,
                       public webui_browser::mojom::PageHandlerFactory {
 public:
  explicit WebUIBrowserUI(content::WebUI* web_ui);
  ~WebUIBrowserUI() override;

  void BindInterface(
      mojo::PendingReceiver<webui_browser::mojom::PageHandlerFactory> receiver);
  void BindInterface(mojo::PendingReceiver<searchbox::mojom::PageHandler>
                         pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver);
  void BindInterface(
      mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver);

  void BindInterface(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver);

  void ShowSidePanel(SidePanelEntryKey side_panel_entry_key);
  void CloseSidePanel();

  Browser* browser() { return browser_; }
  WebUIBrowserWindow* browser_window() {
    return static_cast<WebUIBrowserWindow*>(browser_->window());
  }

  base::WeakPtr<WebUIBrowserUI> GetWeakPtr();

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
  // webui_browser::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver)
      override;

  MetricsReporter metrics_reporter_;
  std::unique_ptr<RealboxHandler> realbox_handler_;

  mojo::Receiver<webui_browser::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  // TODO(webium): this is for testing guest contents embedding. Remove once
  // the tab strip is integrated.
  std::unique_ptr<content::WebContents> test_guest_contents_;

  raw_ptr<Browser> browser_;

  base::WeakPtrFactory<WebUIBrowserUI> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_
