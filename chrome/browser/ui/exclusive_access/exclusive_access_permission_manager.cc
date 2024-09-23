// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_permission_manager.h"

#include "base/time/time.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace {

// We will gather requests for this amount of time, then send them all at once.
constexpr int kRequestDelayMs = 100;

}  // namespace

ExclusiveAccessPermissionManager::ExclusiveAccessPermissionManager(
    ExclusiveAccessContext* exclusive_access_context)
    : exclusive_access_context_(exclusive_access_context) {}

ExclusiveAccessPermissionManager::~ExclusiveAccessPermissionManager() = default;

void ExclusiveAccessPermissionManager::QueuePermissionRequest(
    blink::PermissionType permission_type,
    base::OnceClosure granted_callback,
    base::OnceClosure denied_callback,
    content::WebContents* web_contents) {
  if (!web_contents) {
    std::move(denied_callback).Run();
    return;
  }
  content::GlobalRenderFrameHostId rfh_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();
  if (!pending_requests_.contains(rfh_id)) {
    pending_requests_.emplace(std::piecewise_construct,
                              std::forward_as_tuple(rfh_id),
                              std::forward_as_tuple());
  }
  PendingRequests& requests = pending_requests_.find(rfh_id)->second;
  requests.pending.emplace_back(permission_type, std::move(granted_callback),
                                std::move(denied_callback));

  if (requests.waiting_responses > 0) {
    // We've already started making requests, so send the new request
    // immediately.
    RequestPermissions(rfh_id, web_contents->GetWeakPtr());
  } else if (requests.pending.size() == 1) {
    // This is the first (recent) request for `web_contents`, so wait to see if
    // more requests come in, and then make the request.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExclusiveAccessPermissionManager::RequestPermissions,
                       weak_factory_.GetWeakPtr(), rfh_id,
                       web_contents->GetWeakPtr()),
        base::Milliseconds(kRequestDelayMs));
  } else {
    // No-op, because the request will be handled by RequestPermissions() that
    // was already posted by an earlier request.
  }
}

ExclusiveAccessPermissionManager::PermissionRequest::PermissionRequest(
    blink::PermissionType type,
    base::OnceClosure granted_callback,
    base::OnceClosure denied_callback)
    : type(type),
      granted_callback(std::move(granted_callback)),
      denied_callback(std::move(denied_callback)) {}

ExclusiveAccessPermissionManager::PermissionRequest::PermissionRequest(
    PermissionRequest&& other)
    : PermissionRequest(other.type,
                        std::move(other.granted_callback),
                        std::move(other.denied_callback)) {}

ExclusiveAccessPermissionManager::PermissionRequest::~PermissionRequest() =
    default;

ExclusiveAccessPermissionManager::PendingRequests::PendingRequests() = default;

ExclusiveAccessPermissionManager::PendingRequests::~PendingRequests() = default;

void ExclusiveAccessPermissionManager::RequestPermissions(
    content::GlobalRenderFrameHostId rfh_id,
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    pending_requests_.erase(rfh_id);
    return;
  }
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  auto requests_it = pending_requests_.find(rfh_id);
  if (requests_it == pending_requests_.end()) {
    return;
  }
  PendingRequests& requests = requests_it->second;
  requests.waiting_responses += requests.pending.size();
  for (PermissionRequest& request : requests.pending) {
    content::PermissionRequestDescription description(
        request.type, web_contents->HasRecentInteraction());
    GetPermissionController(web_contents.get())
        ->RequestPermissionsFromCurrentDocument(
            rfh, std::move(description),
            base::BindOnce(
                &ExclusiveAccessPermissionManager::HandleRequestResult,
                weak_factory_.GetWeakPtr(), rfh_id, web_contents,
                std::move(request.granted_callback),
                std::move(request.denied_callback)));
  }
  // `requests_it` may have been invalidated by HandleRequestResult(), so
  // we must re-fetch it.
  requests_it = pending_requests_.find(rfh_id);
  if (requests_it != pending_requests_.end()) {
    requests_it->second.pending.clear();
  }
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(web_contents.get());
  if (media_session) {
    media_session->StartDucking();
  }
}

void ExclusiveAccessPermissionManager::HandleRequestResult(
    content::GlobalRenderFrameHostId rfh_id,
    base::WeakPtr<content::WebContents> web_contents,
    base::OnceClosure granted_callback,
    base::OnceClosure denied_callback,
    const std::vector<blink::mojom::PermissionStatus>& status) {
  auto requests_it = pending_requests_.find(rfh_id);
  if (requests_it == pending_requests_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  PendingRequests& requests = requests_it->second;
  CHECK_EQ(status.size(), 1u);
  std::vector<base::OnceClosure> result_callbacks;
  switch (status[0]) {
    case blink::mojom::PermissionStatus::GRANTED:
      requests.result_callbacks.push_back(std::move(granted_callback));
      break;
    case blink::mojom::PermissionStatus::DENIED:
    case blink::mojom::PermissionStatus::ASK:
      requests.result_callbacks.push_back(std::move(denied_callback));
      break;
  }
  if (requests.result_callbacks.size() != requests.waiting_responses) {
    return;
  }
  // We queue the granted/denied callbacks until all the requests have been
  // responded to, then call them all at once. If we don't do this, in the
  // case where both pointer lock and keyboard lock are requested but the
  // pointer lock permission has already been granted, pointer can be locked
  // first and then immediately unlocked by the permission prompt for keyboard
  // lock.
  for (base::OnceClosure& result_callback : requests.result_callbacks) {
    std::move(result_callback).Run();
  }
  pending_requests_.erase(requests_it);

  content::MediaSession* media_session =
      web_contents ? content::MediaSession::GetIfExists(web_contents.get())
                   : nullptr;
  if (media_session) {
    media_session->StopDucking();
  }
}

content::PermissionController*
ExclusiveAccessPermissionManager::GetPermissionController(
    content::WebContents* web_contents) {
  if (permission_controller_for_test_) {
    return permission_controller_for_test_;
  }
  return web_contents->GetBrowserContext()->GetPermissionController();
}
