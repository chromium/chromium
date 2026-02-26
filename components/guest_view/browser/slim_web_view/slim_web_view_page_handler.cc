// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_page_handler.h"

#include <utility>

#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_permission_helper.h"
#include "content/public/browser/render_process_host.h"

namespace guest_view {

DOCUMENT_USER_DATA_KEY_IMPL(SlimWebViewPageHandler);

SlimWebViewPageHandler::~SlimWebViewPageHandler() = default;

void SlimWebViewPageHandler::DispatchEvent(const std::string& event_name,
                                           base::DictValue args,
                                           int instance_id) {
  page_->DispatchEvent(event_name, std::move(args), instance_id);
}

void SlimWebViewPageHandler::CreateGuest(base::DictValue create_params,
                                         CreateGuestCallback callback) {
  auto* guest_view_manager = GetGuestViewManager();
  if (!guest_view_manager) {
    return;
  }
  auto create_guest_callback =
      base::BindOnce([](guest_view::GuestViewBase* guest) {
        return guest ? guest->guest_instance_id() : kInstanceIDNone;
      }).Then(std::move(callback));
  guest_view_manager->CreateGuest(SlimWebViewGuest::Type, &render_frame_host(),
                                  create_params,
                                  std::move(create_guest_callback));
}

void SlimWebViewPageHandler::SetSize(int32_t guest_instance_id,
                                     mojom::SetSizeParamsPtr size_params) {
  auto* guest = SlimWebViewGuest::FromInstanceID(
      render_frame_host().GetProcess()->GetID(), guest_instance_id);
  if (!guest) {
    mojo::ReportBadMessage("Invalid guest instance id.");
    return;
  }
  guest_view::SetSizeParams params;
  params.enable_auto_size = size_params->enable_auto_size;
  params.min_size.emplace(size_params->min.width(), size_params->min.height());
  params.max_size.emplace(size_params->max.width(), size_params->max.height());
  guest->SetSize(params);
}

void SlimWebViewPageHandler::Navigate(int32_t guest_instance_id,
                                      const GURL& url) {
  auto* guest = SlimWebViewGuest::FromInstanceID(
      render_frame_host().GetProcess()->GetID(), guest_instance_id);
  if (!guest) {
    mojo::ReportBadMessage("Invalid guest instance id.");
    return;
  }
  if (!url.is_valid()) {
    mojo::ReportBadMessage("Invalid URL.");
    return;
  }
  if (!url.SchemeIsHTTPOrHTTPS() && !url.IsAboutBlank()) {
    mojo::ReportBadMessage("URL must be https, http, or about:blank.");
    return;
  }
  guest->Navigate(url);
}

void SlimWebViewPageHandler::SetPermission(
    int32_t guest_instance_id,
    int32_t request_id,
    mojom::PageHandler_PermissionResponseAction action,
    SetPermissionCallback callback) {
  auto* guest = SlimWebViewGuest::FromInstanceID(
      render_frame_host().GetProcess()->GetID(), guest_instance_id);
  if (!guest) {
    mojo::ReportBadMessage("Invalid guest instance id.");
    return;
  }
  auto result = guest->permission_helper().SetPermission(request_id, action);
  if (result == SlimWebViewPermissionHelper::SetPermissionResult::kInvalid) {
    mojo::ReportBadMessage("Invalid request id.");
    return;
  }
  std::move(callback).Run(
      result == SlimWebViewPermissionHelper::SetPermissionResult::kAllowed);
}

SlimWebViewPageHandler::SlimWebViewPageHandler(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<PageHandler> page_handler,
    mojo::PendingRemote<mojom::Page> page)
    : content::DocumentUserData<SlimWebViewPageHandler>(render_frame_host),
      receiver_(this, std::move(page_handler)),
      page_(std::move(page)) {}

GuestViewManager* SlimWebViewPageHandler::GetGuestViewManager() {
  auto* browser_context = render_frame_host().GetBrowserContext();
  auto* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context);
  if (!guest_view_manager) {
    // The guest_view_manager should have been created when calling
    // chrome.slimWebViewPrivate.registerView.
    mojo::ReportBadMessage("View was not registered.");
  }
  return guest_view_manager;
}

}  // namespace guest_view
