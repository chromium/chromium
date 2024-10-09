// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

namespace {

using RequestResult = WebUIContentsPreloadManager::RequestResult;

bool IsEscapeEvent(const input::NativeWebKeyboardEvent& event) {
  return event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

RequestResult Request(const GURL& webui_url,
                      content::BrowserContext* browser_context) {
  auto* preload_manager = WebUIContentsPreloadManager::GetInstance();
  CHECK(preload_manager);
  return preload_manager->Request(webui_url, browser_context);
}

// Enables the web contents to automatically resize to its content and
// notify its delegate.
void EnableAutoResizeForWebContents(content::WebContents* web_contents) {
  if (content::RenderWidgetHostView* render_widget_host_view =
          web_contents->GetRenderWidgetHostView()) {
    render_widget_host_view->EnableAutoResize(gfx::Size(1, 1),
                                              gfx::Size(INT_MAX, INT_MAX));
  }
}

// Enables the web contents to support web platform defined draggable regions
// for the current primary render frame host. This should be called each time
// the primary rfh changes (after navigation for e.g.).
void EnableDraggableRegions(content::WebContents* web_contents) {
  if (content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame()) {
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetSupportsDraggableRegions(true);
  }
}

}  // namespace

bool WebUIContentsWrapper::Host::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

bool WebUIContentsWrapper::Host::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

content::WebContents* WebUIContentsWrapper::Host::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return nullptr;
}

WebUIContentsWrapper::WebUIContentsWrapper(const GURL& webui_url,
                                           Profile* profile,
                                           int task_manager_string_id,
                                           bool webui_resizes_host,
                                           bool esc_closes_ui,
                                           bool supports_draggable_regions,
                                           const std::string& webui_name)
    : webui_resizes_host_(webui_resizes_host),
      esc_closes_ui_(esc_closes_ui),
      supports_draggable_regions_(supports_draggable_regions) {
  RequestResult make_contents_result = Request(webui_url, profile);
  web_contents_ = std::move(make_contents_result.web_contents);
  is_ready_to_show_ = make_contents_result.is_ready_to_show;

  web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(web_contents_.get());

  PrefsTabHelper::CreateForWebContents(web_contents_.get());
  chrome::InitializePageLoadMetricsForWebContents(web_contents_.get());
  task_manager::WebContentsTags::CreateForToolContents(web_contents_.get(),
                                                       task_manager_string_id);
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents_.get());
  }

  if (webui_resizes_host_) {
    EnableAutoResizeForWebContents(web_contents_.get());
  }
  if (supports_draggable_regions_) {
    EnableDraggableRegions(web_contents_.get());
  }

  profile_observation_.Observe(profile);
}

WebUIContentsWrapper::~WebUIContentsWrapper() {
  WebContentsObserver::Observe(nullptr);
}

void WebUIContentsWrapper::ResizeDueToAutoResize(content::WebContents* source,
                                                  const gfx::Size& new_size) {
  DCHECK_EQ(web_contents(), source);
  if (host_)
    host_->ResizeDueToAutoResize(source, new_size);
}

content::KeyboardEventProcessingResult
WebUIContentsWrapper::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  DCHECK_EQ(web_contents(), source);
  // Close the bubble if an escape event is detected. Handle this here to
  // prevent the renderer from capturing the event and not propagating it up.
  if (host_ && IsEscapeEvent(event) && esc_closes_ui_) {
    host_->CloseUI();
    return content::KeyboardEventProcessingResult::HANDLED;
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool WebUIContentsWrapper::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  DCHECK_EQ(web_contents(), source);
  return host_ ? host_->HandleKeyboardEvent(source, event) : false;
}

bool WebUIContentsWrapper::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return host_ ? host_->HandleContextMenu(render_frame_host, params) : true;
}

std::unique_ptr<content::EyeDropper> WebUIContentsWrapper::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  BrowserWindow* window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents_.get());
  return window->OpenEyeDropper(frame, listener);
}

content::WebContents* WebUIContentsWrapper::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return host_ ? host_->OpenURLFromTab(source, params,
                                       std::move(navigation_handle_callback))
               : nullptr;
}

void WebUIContentsWrapper::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (host_) {
    host_->RequestMediaAccessPermission(web_contents, request,
                                        std::move(callback));
  }
}

void WebUIContentsWrapper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  if (host_) {
    host_->RunFileChooser(render_frame_host, listener, params);
  }
}

void WebUIContentsWrapper::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  // Persist regions to allow support transfer between hosts.
  draggable_regions_.emplace();
  base::ranges::transform(regions,
                          std::back_inserter(draggable_regions_.value()),
                          &blink::mojom::DraggableRegionPtr::Clone);
  if (host_) {
    host_->DraggableRegionsChanged(regions, contents);
  }
}

void WebUIContentsWrapper::SetContentsBounds(content::WebContents* source,
                                             const gfx::Rect& bounds) {
  if (host_) {
    host_->SetContentsBounds(source, bounds);
  }
}

void WebUIContentsWrapper::PrimaryPageChanged(content::Page& page) {
  if (webui_resizes_host_) {
    EnableAutoResizeForWebContents(web_contents_.get());
  }
  if (supports_draggable_regions_) {
    draggable_regions_.reset();
    EnableDraggableRegions(web_contents_.get());
  }
}

void WebUIContentsWrapper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  CloseUI();
}

void WebUIContentsWrapper::OnProfileWillBeDestroyed(Profile* profile) {
  web_contents_.reset();
  profile_observation_.Reset();
}

void WebUIContentsWrapper::ShowUI() {
  if (host_) {
    host_->ShowUI();
  }

  // The host should never proactively show the contents after the initial
  // show, in which case the contents could have already been preloaded.
  is_ready_to_show_ = false;
}

void WebUIContentsWrapper::CloseUI() {
  if (host_)
    host_->CloseUI();
}

void WebUIContentsWrapper::ShowContextMenu(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  if (host_)
    host_->ShowCustomContextMenu(point, std::move(menu_model));
}

void WebUIContentsWrapper::HideContextMenu() {
  if (host_)
    host_->HideCustomContextMenu();
}

base::WeakPtr<WebUIContentsWrapper::Host> WebUIContentsWrapper::GetHost() {
  return host_;
}

void WebUIContentsWrapper::SetHost(
    base::WeakPtr<WebUIContentsWrapper::Host> host) {
  DCHECK(!web_contents_->IsCrashed());
  host_ = std::move(host);
  if (!host_) {
    return;
  }

  // Resize the host to the frame size. If there are new updates to the frame
  // size they will be capture by WebUIContentsWrapper::ResizeDueToAutoResize().
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  if (webui_resizes_host_ && rfh && rfh->GetFrameSize().has_value()) {
    // RenderFrameHost::GetFrameSize() returns the actual frame size while
    // the host view expects device-independent size.
    const gfx::Size frame_dip_size = gfx::ScaleToCeiledSize(
        *rfh->GetFrameSize(), 1.f / rfh->GetView()->GetDeviceScaleFactor());
    host_->ResizeDueToAutoResize(web_contents_.get(), frame_dip_size);
  }

  if (supports_draggable_regions_ && draggable_regions_.has_value()) {
    host_->DraggableRegionsChanged(draggable_regions_.value(),
                                   web_contents_.get());
  }
}

void WebUIContentsWrapper::SetWebContentsForTesting(
    std::unique_ptr<content::WebContents> web_contents) {
  web_contents_->SetDelegate(nullptr);
  web_contents_ = std::move(web_contents);
  web_contents_->SetDelegate(this);
}
