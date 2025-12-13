// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_

#include <memory>

#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui_browser/bookmark_bar.mojom.h"
#include "chrome/browser/ui/webui_browser/browser.mojom.h"
#include "chrome/browser/ui/webui_browser/extensions_bar.mojom.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"

class Browser;

namespace content {
class BrowserContext;
}  // namespace content

namespace searchbox::mojom {
class PageHandler;
}  // namespace searchbox::mojom

namespace ui {
class TrackedElementHandler;
}  // namespace ui

class RealboxHandler;
class WebUIBrowserUI;
class WebUIBrowserBookmarkBarPageHandler;

class WebUIBrowserUIConfig
    : public content::DefaultWebUIConfig<WebUIBrowserUI> {
 public:
  WebUIBrowserUIConfig();

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://webui-browser
class WebUIBrowserUI : public ui::MojoWebUIController,
                       public webui_browser::mojom::PageHandlerFactory,
                       public bookmark_bar::mojom::PageHandlerFactory,
                       public extensions_bar::mojom::PageHandlerFactory {
 public:
  explicit WebUIBrowserUI(content::WebUI* web_ui);
  ~WebUIBrowserUI() override;

  void BindInterface(
      mojo::PendingReceiver<webui_browser::mojom::PageHandlerFactory> receiver);
  void BindInterface(
      mojo::PendingReceiver<bookmark_bar::mojom::PageHandlerFactory> receiver);
  void BindInterface(
      mojo::PendingReceiver<extensions_bar::mojom::PageHandlerFactory>
          receiver);
  void BindInterface(mojo::PendingReceiver<searchbox::mojom::PageHandler>
                         pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver);
  void BindInterface(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver);
  void BindInterface(
      mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
          receiver);

  void BookmarkBarStateChanged(BookmarkBar::AnimateChangeType change_type);
  void ShowSidePanel(SidePanelEntryKey side_panel_entry_key);
  void CloseSidePanel();

  Browser* browser() { return browser_; }
  WebUIBrowserWindow* browser_window() {
    return static_cast<WebUIBrowserWindow*>(browser_->window());
  }

  webui_browser::mojom::Page* page() {
    // get() should only be called if bound, so check first.
    return page_.is_bound() ? page_.get() : nullptr;
  }

  base::WeakPtr<WebUIBrowserUI> GetWeakPtr();

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
  // webui_browser::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<webui_browser::mojom::Page> page,
      mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver)
      override;
  void GetTabStripInset(GetTabStripInsetCallback callback) override;

  // bookmark_bar::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<bookmark_bar::mojom::Page> page,
                         mojo::PendingReceiver<bookmark_bar::mojom::PageHandler>
                             receiver) override;

  // extensions_bar::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<extensions_bar::mojom::Page> page,
      mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver)
      override;

  // Returns the list of known element identifiers. These elements are HTML
  // elements tracked by ui/webui/tracked_element. Used for anchoring secondary
  // UIs.
  const std::vector<ui::ElementIdentifier>& GetKnownElementIdentifiers() const;

  std::unique_ptr<RealboxHandler> realbox_handler_;
  std::unique_ptr<WebUIBrowserBookmarkBarPageHandler>
      bookmark_bar_page_handler_;
  std::unique_ptr<ui::TrackedElementHandler> tracked_element_handler_;

  mojo::Remote<webui_browser::mojom::Page> page_;
  mojo::Receiver<webui_browser::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  mojo::Receiver<bookmark_bar::mojom::PageHandlerFactory>
      bookmark_bar_page_factory_receiver_{this};
  mojo::Receiver<extensions_bar::mojom::PageHandlerFactory>
      extensions_bar_page_factory_receiver_{this};

  raw_ptr<Browser> browser_;

  base::WeakPtrFactory<WebUIBrowserUI> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_
