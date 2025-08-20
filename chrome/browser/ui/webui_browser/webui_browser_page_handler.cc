// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_page_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_api/types/node_id.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace {

content::NavigationController* GetGuestNavigationController(int guest_id) {
  auto* guest_handle = guest_contents::GuestContentsHandle::FromID(guest_id);
  if (!guest_handle) {
    return nullptr;
  }
  auto* web_contents = guest_handle->web_contents();
  return web_contents ? &web_contents->GetController() : nullptr;
}

}  // namespace

WebUIBrowserPageHandler::~WebUIBrowserPageHandler() = default;

// static
void WebUIBrowserPageHandler::CreateForRenderFrameHost(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver,
    WebUIBrowserUI* controller) {
  // The RenderFrameHost takes ownership of this object via the DocumentService.
  new WebUIBrowserPageHandler(render_frame_host, std::move(receiver),
                              controller);
}

void WebUIBrowserPageHandler::GetGuestIdForTabId(
    const tabs_api::NodeId& tab_id,
    GetGuestIdForTabIdCallback callback) {
  tabs::TabInterface* tab = nullptr;
  std::optional<tabs::TabHandle> maybe_tab_handle = tab_id.ToTabHandle();
  if (maybe_tab_handle) {
    tab = maybe_tab_handle->Get();
  }

  if (!tab) {
    mojo::ReportBadMessage("Invalid tab id");
    return;
  }
  content::WebContents* tab_contents = tab->GetContents();
  if (tab_contents == nullptr) {
    mojo::ReportBadMessage("Tab has no contents");
    return;
  }
  guest_contents::GuestContentsHandle::CreateForWebContents(tab_contents);
  auto* guest_handle =
      guest_contents::GuestContentsHandle::FromWebContents(tab_contents);
  std::move(callback).Run(guest_handle->id());
}

void WebUIBrowserPageHandler::LoadTabSearch(LoadTabSearchCallback callback) {
  content::WebContents::CreateParams params(GetBrowser()->profile());
  tab_search_contents_ = content::WebContents::Create(params);
  tab_search_contents_->SetColorProviderSource(GetBrowserWindow());
  content::NavigationController::LoadURLParams url_params{
      GURL(chrome::kChromeUITabSearchURL)};
  tab_search_contents_->GetController().LoadURLWithParams(url_params);
  guest_contents::GuestContentsHandle::CreateForWebContents(
      tab_search_contents_.get());
  auto* guest_handle = guest_contents::GuestContentsHandle::FromWebContents(
      tab_search_contents_.get());
  std::move(callback).Run(guest_handle->id());
}

void WebUIBrowserPageHandler::ShowTabSearchBubble(
    const std::string& anchor_name) {
  // TODO(webium): Call TabSearchBubbleHost::ShowTabSearchBubble().
  NOTIMPLEMENTED();
}

void WebUIBrowserPageHandler::Navigate(int guest_id, const GURL& src) {
  content::NavigationController::LoadURLParams load_url_params(src);
  auto* navigation_controller = GetGuestNavigationController(guest_id);
  if (navigation_controller) {
    navigation_controller->LoadURLWithParams(load_url_params);
  } else {
    mojo::ReportBadMessage("Invalid guest id");
  }
}

void WebUIBrowserPageHandler::CanGoBack(int guest_id,
                                        CanGoBackCallback callback) {
  auto* navigation_controller = GetGuestNavigationController(guest_id);
  std::move(callback).Run(
      navigation_controller ? navigation_controller->CanGoBack() : false);
}

void WebUIBrowserPageHandler::GoBack(int guest_id) {
  auto* navigation_controller = GetGuestNavigationController(guest_id);
  if (navigation_controller) {
    if (navigation_controller->CanGoBack()) {
      navigation_controller->GoBack();
    }
  } else {
    mojo::ReportBadMessage("Invalid guest id");
  }
}

void WebUIBrowserPageHandler::CanGoForward(int guest_id,
                                           CanGoForwardCallback callback) {
  auto* navigation_controller = GetGuestNavigationController(guest_id);
  std::move(callback).Run(
      navigation_controller ? navigation_controller->CanGoForward() : false);
}

void WebUIBrowserPageHandler::GoForward(int guest_id) {
  auto* navigation_controller = GetGuestNavigationController(guest_id);
  if (navigation_controller) {
    if (navigation_controller->CanGoForward()) {
      navigation_controller->GoForward();
    }
  } else {
    mojo::ReportBadMessage("Invalid guest id");
  }
}

void WebUIBrowserPageHandler::OpenAppMenu() {
  menu_.reset();

  // TODO(webium): use BrowserElements::From(browser)->GetElement(). This
  // requires adding a BrowserElementsWebUI.
  ui::TrackedElement* app_menu_button =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kToolbarAppMenuButtonElementId,
          ui::ElementContext(GetBrowserWindow()->widget()));
  CHECK(app_menu_button) << "App menu button not found";
  menu_model_ =
      std::make_unique<AppMenuModel>(GetBrowserWindow(), GetBrowser());
  menu_model_->Init();
  menu_ = std::make_unique<AppMenu>(GetBrowser(), menu_model_.get(),
                                    views::MenuRunner::NO_FLAGS);
  menu_->RunMenu(GetBrowserWindow()->widget(),
                 app_menu_button->GetScreenBounds());
}

void WebUIBrowserPageHandler::OpenProfileMenu() {
  // TODO(webium): Find profile menu button and call
  // GetBrowser()->GetFeatures().profile_menu_coordinator()->Show(
  //    /*is_source_accelerator=*/false, avatar_button);
  NOTIMPLEMENTED();
}

void WebUIBrowserPageHandler::LaunchDevToolsForBrowser() {
  content::WebContents* webcontents_to_inspect =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  DevToolsWindow::OpenDevToolsWindow(
      webcontents_to_inspect, DevToolsOpenedByAction::kMainMenuOrMainShortcut);
}

void WebUIBrowserPageHandler::OnSidePanelClosed() {
  // TODO(webium): Find side panel UI and call OnSidePanelClosed().
  NOTIMPLEMENTED();
}

void WebUIBrowserPageHandler::Minimize() {
  GetBrowserWindow()->Minimize();
}

void WebUIBrowserPageHandler::Maximize() {
  GetBrowserWindow()->Maximize();
}

void WebUIBrowserPageHandler::Restore() {
  GetBrowserWindow()->Restore();
}

void WebUIBrowserPageHandler::Close() {
  GetBrowserWindow()->Close();
}

WebUIBrowserPageHandler::WebUIBrowserPageHandler(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver,
    WebUIBrowserUI* controller)
    : content::DocumentService<webui_browser::mojom::PageHandler>(
          render_frame_host,
          std::move(receiver)),
      controller_(controller->GetWeakPtr()) {}

Browser* WebUIBrowserPageHandler::GetBrowser() {
  if (!controller_) {
    return nullptr;
  }

  return controller_->browser();
}

WebUIBrowserWindow* WebUIBrowserPageHandler::GetBrowserWindow() {
  if (!controller_) {
    return nullptr;
  }

  return controller_->browser_window();
}
