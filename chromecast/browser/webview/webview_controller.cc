// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_controller.h"

#include <set>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/cast_web_preferences.h"
#include "chromecast/browser/web_types.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_navigation_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

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

CastWebPreferences* GetCastPreferencesFor(content::WebContents* web_contents) {
  return static_cast<CastWebPreferences*>(web_contents->GetUserData(
      CastWebPreferences::kCastWebPreferencesDataKey));
}

void UpdateWebkitPreferences(content::WebContents* web_contents,
                             CastWebPreferences* cast_prefs) {
  blink::web_pref::WebPreferences prefs =
      web_contents->GetOrCreateWebPreferences();
  cast_prefs->Update(&prefs);
  web_contents->SetWebPreferences(prefs);
}

}  // namespace

WebviewController::WebviewController(
    std::unique_ptr<content::BrowserContext> browser_context,
    Client* client,
    bool enabled_for_dev)
    : WebviewController(browser_context.get(), client, enabled_for_dev) {
  owned_context_ = std::move(browser_context);
}

WebviewController::WebviewController(content::BrowserContext* browser_context,
                                     Client* client,
                                     bool enabled_for_dev)
    : WebContentController(client), enabled_for_dev_(enabled_for_dev) {
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  contents_ = content::WebContents::Create(create_params);
  contents_->SetUserData(kWebviewResponseUserDataKey,
                         std::make_unique<WebviewUserData>(this));
  contents_->SetUserData(CastWebPreferences::kCastWebPreferencesDataKey,
                         std::make_unique<CastWebPreferences>());

  CastWebPreferences* cast_prefs = GetCastPreferencesFor(contents_.get());

  // Allow Webviews to show scrollbars. These are globally disabled since Cast
  // Apps are not expected to be scrollable.
  cast_prefs->preferences()->hide_scrollbars = false;

  // Disallow Webviews to use multiple windows to show the new page in the
  // existing view.
  cast_prefs->preferences()->supports_multiple_windows = false;

  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->is_root_window = true;
  params->enabled_for_dev = enabled_for_dev;
  cast_web_contents_ =
      std::make_unique<CastWebContentsImpl>(contents_.get(), std::move(params));
  CastWebContentsObserver::Observe(cast_web_contents_.get());

  content::WebContentsObserver::Observe(contents_.get());

  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  // For webviews, set the ax_id to be the cast_web_contents' id
  // rather than the ax tree id for the main frame. The main frame can be
  // replaced after we've set this from navigation. Prefix the string with
  // "T:" to tell the ax bridge to find the cast_web_contents by id.
  // Then it can find the current ax tree id from that.
  std::string ax_id = "T:" + base::NumberToString(cast_web_contents_->id());
  response->mutable_create_response()
      ->mutable_accessibility_info()
      ->set_ax_tree_id(ax_id);
  client->EnqueueSend(std::move(response));
}

WebviewController::~WebviewController() {
  CastWebContentsObserver::Observe(nullptr);
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
        handle,
        webview_user_data->controller()->weak_ptr_factory_.GetWeakPtr());
  }
  return nullptr;
}

void WebviewController::ProcessRequest(const webview::WebviewRequest& request) {
  switch (request.type_case()) {
    case webview::WebviewRequest::kNavigate:
      if (request.has_navigate()) {
        HandleLoadUrl(request.navigate());
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

    case webview::WebviewRequest::kUpdateSettings:
      if (request.has_update_settings()) {
        HandleUpdateSettings(request.update_settings());
      } else {
        client_->OnError("update_settings() not supplied");
      }
      break;

    case webview::WebviewRequest::kSetAutoMediaPlaybackPolicy:
      if (request.has_set_auto_media_playback_policy()) {
        HandleSetAutoMediaPlaybackPolicy(
            request.set_auto_media_playback_policy());
      } else {
        client_->OnError("set_auto_media_playback_policy() not supplied");
      }
      break;

    default:
      WebContentController::ProcessRequest(request);
      break;
  }
}

void WebviewController::HandleLoadUrl(const webview::NavigateRequest& request) {
  LOG(INFO) << "Navigate webview to " << request.url();
  stopped_ = false;

  content::NavigationController::LoadURLParams params(GURL(request.url()));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.override_user_agent = content::NavigationController::UA_OVERRIDE_TRUE;
  GetWebContents()->GetController().LoadURLWithParams(params);
}

void WebviewController::HandleUpdateSettings(
    const webview::UpdateSettingsRequest& request) {
  content::WebContents* contents = GetWebContents();
  CastWebPreferences* cast_prefs = GetCastPreferencesFor(contents);

  cast_prefs->preferences()->javascript_enabled = request.javascript_enabled();
  UpdateWebkitPreferences(contents, cast_prefs);

  has_navigation_delegate_ = request.has_navigation_delegate();

  CastWebContents::FromWebContents(contents)->SetEnabledForRemoteDebugging(
      request.debugging_enabled() || enabled_for_dev_);

  const bool user_agent_overridden =
      request.has_user_agent() &&
      request.user_agent().type_case() == webview::UserAgent::kValue;

  if (user_agent_overridden) {
    contents->SetUserAgentOverride(
        blink::UserAgentOverride::UserAgentOnly(request.user_agent().value()),
        true);
  }

  content::NavigationController& controller = contents->GetController();
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    controller.GetEntryAtIndex(i)->SetIsOverridingUserAgent(
        user_agent_overridden);
  }
}

void WebviewController::HandleSetAutoMediaPlaybackPolicy(
    const webview::SetAutoMediaPlaybackPolicyRequest& request) {
  content::WebContents* contents = GetWebContents();
  CastWebPreferences* cast_prefs = GetCastPreferencesFor(contents);

  cast_prefs->preferences()->autoplay_policy =
      request.require_user_gesture()
          ? blink::mojom::AutoplayPolicy::kUserGestureRequired
          : blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  UpdateWebkitPreferences(contents, cast_prefs);
}

void WebviewController::DidFirstVisuallyNonEmptyPaint() {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    event->set_did_first_visually_non_empty_paint(true);
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::SendNavigationEvent(
    WebviewNavigationThrottle* throttle,
    content::NavigationHandle* navigation_handle) {
  if (current_navigation_throttle_) {
    current_navigation_throttle_->ProcessNavigationDecision(
        webview::NavigationDecision::PREVENT);
    current_navigation_throttle_ = nullptr;
  }

  DCHECK(navigation_handle);
  if (!client_) {
    DLOG(INFO)
        << "Attempt to dispatch navigation event after RPC client invalidation";
    return;
  }

  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  auto* navigation_event = response->mutable_navigation_event();

  navigation_event->set_url(navigation_handle->GetURL().spec());
  navigation_event->set_is_for_main_frame(navigation_handle->IsInMainFrame());
  navigation_event->set_is_renderer_initiated(
      navigation_handle->IsRendererInitiated());
  navigation_event->set_is_same_document(navigation_handle->IsSameDocument());
  navigation_event->set_has_user_gesture(navigation_handle->HasUserGesture());
  navigation_event->set_was_server_redirect(
      navigation_handle->WasServerRedirect());
  navigation_event->set_is_post(navigation_handle->IsPost());
  navigation_event->set_was_initiated_by_link_click(
      navigation_handle->WasInitiatedByLinkClick());

  current_navigation_throttle_ = throttle;
  client_->EnqueueSend(std::move(response));
}

void WebviewController::OnNavigationThrottleDestroyed(
    WebviewNavigationThrottle* throttle) {
  if (current_navigation_throttle_ == throttle)
    current_navigation_throttle_ = nullptr;
}

void WebviewController::ClosePage() {
  cast_web_contents_->ClosePage();
}

content::WebContents* WebviewController::GetWebContents() {
  return contents_.get();
}

webview::AsyncPageEvent_State WebviewController::current_state() {
  return ToGrpcPageState(page_state_);
}

void WebviewController::PageStateChanged(PageState page_state) {
  page_state_ = page_state;
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::PageStopped(PageState page_state, int error_code) {
  page_state_ = page_state;
  stopped_ = true;
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    event->set_stopped_error_code(error_code);
    event->set_stopped_error_description(net::ErrorToShortString(error_code));
    client_->EnqueueSend(std::move(response));
  } else {
    // Can't destroy in an observer callback, so post a task to do it.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

void WebviewController::ResourceLoadFailed() {
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
