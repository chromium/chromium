// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/webui_browser/browser.mojom.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class AppMenu;
class AppMenuModel;
class Browser;

class WebUIBrowserUI;

class WebUIBrowserPageHandler
    : public content::DocumentService<webui_browser::mojom::PageHandler> {
 public:
  WebUIBrowserPageHandler(const WebUIBrowserPageHandler&) = delete;
  WebUIBrowserPageHandler& operator=(const WebUIBrowserPageHandler&) = delete;
  ~WebUIBrowserPageHandler() override;

  static void CreateForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver,
      WebUIBrowserUI* controller);

  // webui_browser::mojom::PageHandler
  void GetGuestIdForTabId(
      const tabs_api::NodeId& tab_id,
      mojo::PendingReceiver<webui_browser::mojom::GuestHandler> receiver,
      GetGuestIdForTabIdCallback callback) override;
  void LoadTabSearch(LoadTabSearchCallback callback) override;
  void ShowTabSearchBubble(const std::string& anchor_name) override;
  void OpenAppMenu() override;
  void OpenProfileMenu() override;
  void LaunchDevToolsForBrowser() override;
  void OnSidePanelClosed() override;
  void Minimize() override;
  void Maximize() override;
  void Restore() override;
  void Close() override;

 private:
  WebUIBrowserPageHandler(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver,
      WebUIBrowserUI* controller);

  Browser* GetBrowser();
  WebUIBrowserWindow* GetBrowserWindow();

  std::unique_ptr<AppMenuModel> menu_model_;
  std::unique_ptr<AppMenu> menu_;

  std::unique_ptr<content::WebContents> tab_search_contents_;
  std::unique_ptr<TabSearchBubbleHost> tab_search_bubble_host_;

  std::unique_ptr<content::WebContents> glic_contents_;

  // During WebContents destroy, the WebUI object is destroyed before document
  // services, causing a raw_ptr of BrowserUI dangling here, so use a weak ptr
  // instead.
  base::WeakPtr<WebUIBrowserUI> controller_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_PAGE_HANDLER_H_
