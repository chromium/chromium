// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_controller.h"

#include <set>

#include "base/json/json_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_layout_manager.h"
#include "chromecast/browser/webview/webview_navigation_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"

namespace chromecast {

namespace {

const void* kWebviewResponseUserDataKey = &kWebviewResponseUserDataKey;

class WebviewUserData : public base::SupportsUserData::Data {
 public:
  explicit WebviewUserData(WebviewController* controller);
  ~WebviewUserData() override;

  std::unique_ptr<Data> Clone() override;

  WebviewController* controller() const { return controller_; }

 private:
  WebviewController* controller_;
};

}  // namespace

WebviewController::WebviewController(content::BrowserContext* browser_context,
                                     Client* client)
    : WebContentController(client) {
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  contents_ = content::WebContents::Create(create_params);
  contents_->SetUserData(kWebviewResponseUserDataKey,
                         std::make_unique<WebviewUserData>(this));
  CastWebContents::InitParams cast_contents_init;
  cast_contents_init.is_root_window = true;
  cast_contents_init.enabled_for_dev = CAST_IS_DEBUG_BUILD();
  cast_contents_init.delegate = weak_ptr_factory_.GetWeakPtr();
  cast_web_contents_ = std::make_unique<CastWebContentsImpl>(
      contents_.get(), cast_contents_init);
  cast_web_contents_->AddObserver(this);

  content::WebContentsObserver::Observe(contents_.get());

  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  auto ax_id = contents_->GetMainFrame()->GetAXTreeID().ToString();
  response->mutable_create_response()
      ->mutable_accessibility_info()
      ->set_ax_tree_id(ax_id);
  client->EnqueueSend(std::move(response));
}

WebviewController::~WebviewController() {
  cast_web_contents_->RemoveObserver(this);
}

std::unique_ptr<content::NavigationThrottle>
WebviewController::MaybeGetNavigationThrottle(
    content::NavigationHandle* handle) {
  auto* web_contents = handle->GetWebContents();
  auto* webview_user_data = static_cast<WebviewUserData*>(
      web_contents->GetUserData(kWebviewResponseUserDataKey));
  if (webview_user_data &&
      webview_user_data->controller()->has_navigation_delegate_) {
    return std::make_unique<WebviewNavigationThrottle>(
        handle, webview_user_data->controller());
  }
  return nullptr;
}

void WebviewController::ProcessRequest(const webview::WebviewRequest& request) {
  switch (request.type_case()) {
    case webview::WebviewRequest::kNavigate:
      if (request.has_navigate()) {
        LOG(INFO) << "Navigate webview to " << request.navigate().url();
        stopped_ = false;
        cast_web_contents_->LoadUrl(GURL(request.navigate().url()));
      } else {
        client_->OnError("navigate() not supplied");
      }
      break;

    case webview::WebviewRequest::kStopPage:
      if (request.has_stop_page()) {
        cast_web_contents_->Stop(request.stop_page().error_code());
      } else {
        client_->OnError("stop_page() not supplied");
      }
      break;

    case webview::WebviewRequest::kNavigationDecision:
      if (current_navigation_throttle_) {
        current_navigation_throttle_->ProcessNavigationDecision(
            request.navigation_decision());
        current_navigation_throttle_ = nullptr;
      }
      break;

    default:
      WebContentController::ProcessRequest(request);
      break;
  }
}

void WebviewController::SendNavigationEvent(WebviewNavigationThrottle* throttle,
                                            const GURL& gurl,
                                            bool is_in_main_frame) {
  DCHECK(!current_navigation_throttle_);
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  response->mutable_navigation_event()->set_url(gurl.spec());
  response->mutable_navigation_event()->set_is_for_main_frame(is_in_main_frame);
  current_navigation_throttle_ = throttle;
  client_->EnqueueSend(std::move(response));
}

void WebviewController::ClosePage() {
  cast_web_contents_->ClosePage();
}

content::WebContents* WebviewController::GetWebContents() {
  return contents_.get();
}

webview::AsyncPageEvent_State WebviewController::current_state() {
  // The PB enum is defined identically.
  return static_cast<webview::AsyncPageEvent_State>(
      cast_web_contents_->page_state());
}

void WebviewController::OnPageStateChanged(CastWebContents* cast_web_contents) {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::OnPageStopped(CastWebContents* cast_web_contents,
                                      int error_code) {
  stopped_ = true;
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    event->set_stopped_error_code(error_code);
    client_->EnqueueSend(std::move(response));
  } else {
    // Can't destroy in an observer callback, so post a task to do it.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

void WebviewController::ResourceLoadFailed(CastWebContents* cast_web_contents) {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    event->set_resource_load_failed(true);
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::Destroy() {
  // This webview is now abandoned and should close.
  client_ = nullptr;
  js_channels_.reset();
  if (stopped_) {
    // If the page has been stopped this can be deleted immediately.
    delete this;
  } else {
    // This will eventually call OnPageStopped.
    cast_web_contents_->ClosePage();
  }
}

WebviewUserData::WebviewUserData(WebviewController* controller)
    : controller_(controller) {}

WebviewUserData::~WebviewUserData() = default;

std::unique_ptr<base::SupportsUserData::Data> WebviewUserData::Clone() {
  return std::make_unique<WebviewUserData>(controller_);
}

}  // namespace chromecast
