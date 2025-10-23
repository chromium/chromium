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
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/webui_browser/webui_browser_side_panel_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/interaction/element_tracker_views.h"

class WebUIBrowserGuestHandler
    : public content::DocumentService<webui_browser::mojom::GuestHandler> {
 public:
  WebUIBrowserGuestHandler(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<webui_browser::mojom::GuestHandler> receiver,
      content::WebContents* web_contents,
      WebUIBrowserWindow* window)
      : content::DocumentService<webui_browser::mojom::GuestHandler>(
            render_frame_host,
            std::move(receiver)),
        window_(window),
        web_contents_(web_contents->GetWeakPtr()) {}
  WebUIBrowserGuestHandler(const WebUIBrowserGuestHandler&) = delete;
  WebUIBrowserGuestHandler& operator=(const WebUIBrowserGuestHandler&) = delete;
  ~WebUIBrowserGuestHandler() override = default;

 private:
  // webui_browser::mojom::GuestHandler
  void Navigate(const GURL& src) override {
    content::NavigationController::LoadURLParams load_url_params(src);
    web_contents_->GetController().LoadURLWithParams(load_url_params);
  }

  void CanGoBack(CanGoBackCallback callback) override {
    if (!web_contents_.get()) {
      std::move(callback).Run(/*can_go_back=*/false);
      return;
    }
    std::move(callback).Run(web_contents_->GetController().CanGoBack());
  }

  void GoBack() override {
    if (web_contents_->GetController().CanGoBack()) {
      web_contents_->GetController().GoBack();
    }
  }

  void CanGoForward(CanGoForwardCallback callback) override {
    if (!web_contents_.get()) {
      std::move(callback).Run(/*can_go_forward=*/false);
      return;
    }
    std::move(callback).Run(web_contents_->GetController().CanGoForward());
  }

  void GoForward() override {
    if (web_contents_->GetController().CanGoForward()) {
      web_contents_->GetController().GoForward();
    }
  }

  void Reload() override {
    web_contents_->GetController().Reload(content::ReloadType::NORMAL, true);
  }

  void StopLoading() override { web_contents_->Stop(); }

  void OpenPageInfoMenu() override {
    std::unique_ptr<PageInfoBubbleSpecification> specification =
        PageInfoBubbleSpecification::Builder(
            window_->GetLocationBar()->GetChipAnchor()->anchor,
            window_->GetNativeWindow(), web_contents_.get(),
            web_contents_->GetLastCommittedURL())
            .Build();

    views::BubbleDialogDelegateView* const bubble =
        PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));
    bubble->GetWidget()->Show();
  }

  void GetSecurityIcon(GetSecurityIconCallback callback) override {
    auto* icon = &window_->browser()
                      ->GetFeatures()
                      .location_bar_model()
                      ->GetVectorIcon();
    webui_browser::mojom::SecurityIcon icon_type;
    if (icon == &omnibox::kHttpChromeRefreshIcon) {
      icon_type = webui_browser::mojom::SecurityIcon::HttpChromeRefresh;
    } else if (icon == &omnibox::kSecurePageInfoChromeRefreshIcon) {
      icon_type =
          webui_browser::mojom::SecurityIcon::SecurePageInfoChromeRefresh;
    } else if (icon == &vector_icons::kNoEncryptionIcon) {
      icon_type = webui_browser::mojom::SecurityIcon::NoEncryption;
    } else if (icon == &vector_icons::kNotSecureWarningChromeRefreshIcon) {
      icon_type =
          webui_browser::mojom::SecurityIcon::NotSecureWarningChromeRefresh;
    } else if (icon == &vector_icons::kBusinessChromeRefreshIcon) {
      icon_type = webui_browser::mojom::SecurityIcon::BusinessChromeRefresh;
    } else if (icon == &vector_icons::kDangerousChromeRefreshIcon) {
      icon_type = webui_browser::mojom::SecurityIcon::DangerousChromeRefresh;
    } else if (icon == &omnibox::kProductChromeRefreshIcon) {
      icon_type = webui_browser::mojom::SecurityIcon::ProductChromeRefresh;
    } else if (icon == &vector_icons::kExtensionChromeRefreshIcon) {
      icon_type = webui_browser::mojom::SecurityIcon::ExtensionChromeRefresh;
    } else if (icon == &omnibox::kOfflinePinIcon) {
      icon_type = webui_browser::mojom::SecurityIcon::OfflinePin;
    } else {
      CHECK(false) << "Add new icon to webui_browsers's browser.mojom and "
                   << "app.ts and icons.html.ts.";
    }
    std::move(callback).Run(icon_type);
  }

  raw_ptr<WebUIBrowserWindow> window_;

  // The WebContents is destroyed before document
  // services, causing a raw_ptr of WebContents dangling here, so use a weak
  // ptr instead.
  base::WeakPtr<content::WebContents> web_contents_;
};

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
    mojo::PendingReceiver<webui_browser::mojom::GuestHandler> receiver,
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

  // The RenderFrameHost takes ownership of this object via the DocumentService.
  new WebUIBrowserGuestHandler(render_frame_host(), std::move(receiver),
                               tab_contents, GetBrowserWindow());

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

void WebUIBrowserPageHandler::OpenAppMenu() {
  menu_.reset();

  // TODO(webium): use BrowserElements::From(browser)->GetElement(). This
  // requires adding a BrowserElementsWebUI.
  ui::TrackedElement* app_menu_button =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kToolbarAppMenuButtonElementId,
          views::ElementTrackerViews::GetContextForWidget(
              GetBrowserWindow()->widget()));
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
  GetBrowser()->GetFeatures().profile_menu_coordinator()->Show(
      /*is_source_accelerator=*/false);
}

void WebUIBrowserPageHandler::LaunchDevToolsForBrowser() {
  content::WebContents* webcontents_to_inspect =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  DevToolsWindow::OpenDevToolsWindow(
      webcontents_to_inspect, DevToolsOpenedByAction::kMainMenuOrMainShortcut);
}

void WebUIBrowserPageHandler::OnSidePanelClosed() {
  GetBrowserWindow()->GetWebUIBrowserSidePanelUI()->OnSidePanelClosed(
      SidePanelEntry::PanelType::kContent);
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
