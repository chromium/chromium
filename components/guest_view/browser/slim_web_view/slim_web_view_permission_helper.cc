// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_permission_helper.h"

#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view.mojom.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_constants.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace guest_view {

namespace {
constexpr size_t kMaxPendingRequests = 1024;

std::string SlimWebViewPermissionTypeToString(
    SlimWebViewPermissionType permission_type) {
  switch (permission_type) {
    case SlimWebViewPermissionType::kMedia:
      return "media";
    case SlimWebViewPermissionType::kGeolocation:
      return "geolocation";
  }
}

}  // namespace

// static
SlimWebViewPermissionHelper* SlimWebViewPermissionHelper::FromRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  SlimWebViewGuest* guest =
      SlimWebViewGuest::FromRenderFrameHost(render_frame_host);
  return guest ? &guest->permission_helper() : nullptr;
}

struct SlimWebViewPermissionHelper::PermissionResponseInfo {
  PermissionResponseCallback callback;
  SlimWebViewPermissionType permission_type;
  bool allowed_by_default;
};

SlimWebViewPermissionHelper::SlimWebViewPermissionHelper(
    SlimWebViewGuest* guest)
    : guest_(guest) {}

SlimWebViewPermissionHelper::~SlimWebViewPermissionHelper() = default;

void SlimWebViewPermissionHelper::RequestGeolocationPermission(
    const GURL& requesting_frame_url,
    bool user_gesture,
    base::OnceCallback<void(content::PermissionResult)> callback) {
  CHECK(!guest_->IsOwnedByControlledFrameEmbedder());
  base::DictValue request_info;
  request_info.Set(guest_view::kUrl, requesting_frame_url.spec());

  PermissionResponseCallback permission_callback = base::BindOnce(
      &SlimWebViewPermissionHelper::OnGeolocationPermissionResponse,
      weak_factory_.GetWeakPtr(), user_gesture, std::move(callback));
  RequestPermission(SlimWebViewPermissionType::kGeolocation,
                    std::move(request_info), std::move(permission_callback),
                    /*allowed_by_default=*/false);
}

void SlimWebViewPermissionHelper::RequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  base::DictValue request_info;
  request_info.Set(guest_view::kUrl, request.security_origin.spec());
  RequestPermission(
      SlimWebViewPermissionType::kMedia, std::move(request_info),
      base::BindOnce(&SlimWebViewPermissionHelper::OnMediaPermissionResponse,
                     weak_factory_.GetWeakPtr(), request, std::move(callback)),
      /*allowed_by_default=*/false);
}

void SlimWebViewPermissionHelper::RequestPermission(
    SlimWebViewPermissionType permission_type,
    base::DictValue request_info,
    PermissionResponseCallback callback,
    bool allowed_by_default) {
  if (pending_requests_.size() >= kMaxPendingRequests) {
    // Let the stack unwind before we deny the permission request so that
    // objects held by the permission request are not destroyed immediately
    // after creation. This is to allow those same objects to be accessed again
    // in the same scope without fear of use after freeing.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), allowed_by_default));
    return;
  }
  int request_id = ++next_request_id_;
  pending_requests_[request_id] = PermissionResponseInfo{
      std::move(callback), permission_type, allowed_by_default};
  base::DictValue args;
  args.Set(slim_web_view::kRequestInfo, std::move(request_info));
  args.Set(slim_web_view::kRequestId, request_id);
  args.Set(slim_web_view::kPermission,
           SlimWebViewPermissionTypeToString(permission_type));
  guest_->DispatchEventToView(std::make_unique<GuestViewEvent>(
      slim_web_view::kEventPermission, std::move(args)));
}

SlimWebViewPermissionHelper::SetPermissionResult
SlimWebViewPermissionHelper::SetPermission(int request_id,
                                           PermissionResponseAction action) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    return SetPermissionResult::kInvalid;
  }
  PermissionResponseInfo& info = it->second;
  bool allow = (action == PermissionResponseAction::kAllow) ||
               ((action == PermissionResponseAction::kDefault) &&
                info.allowed_by_default);
  std::move(info.callback).Run(allow);

  pending_requests_.erase(it);
  return allow ? SetPermissionResult::kAllowed : SetPermissionResult::kDenied;
}

void SlimWebViewPermissionHelper::OnGeolocationPermissionResponse(
    bool user_gesture,
    base::OnceCallback<void(content::PermissionResult)> callback,
    bool allow) {
  if (!allow) {
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }
  // The <webview> embedder has responded to the permission request. We now need
  // to make sure that the embedder has geolocation permission.
  RequestEmbedderFramePermission(user_gesture, std::move(callback),
                                 blink::PermissionType::GEOLOCATION);
}

void SlimWebViewPermissionHelper::OnMediaPermissionResponse(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    bool allow) {
  if (!allow) {
    std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                            blink::mojom::MediaStreamRequestResult::
                                PERMISSION_DENIED_BY_EMBEDDER_CONTEXT,
                            std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  if (!guest_->attached() || !guest_->embedder_web_contents()->GetDelegate()) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  content::RenderFrameHost* embedder_rfh = guest_->embedder_rfh();
  const url::Origin& embedder_origin = embedder_rfh->GetLastCommittedOrigin();
  // The only client of SlimWebView is Glic, in which requests approved are
  // forwarded as the embedder
  content::MediaStreamRequest embedder_request = request;
  content::GlobalRenderFrameHostId embedder_rfh_id =
      embedder_rfh->GetGlobalId();
  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  embedder_request.render_process_id =
      embedder_rfh_id.child_id.GetUnsafeValue();
  embedder_request.render_frame_id = embedder_rfh_id.frame_routing_id;
  embedder_request.url_origin = embedder_origin;
  embedder_request.security_origin = embedder_origin.GetURL();

  guest_->embedder_web_contents()->GetDelegate()->RequestMediaAccessPermission(
      guest_->embedder_web_contents(), embedder_request, std::move(callback));
}

void SlimWebViewPermissionHelper::RequestEmbedderFramePermission(
    bool user_gesture,
    base::OnceCallback<void(content::PermissionResult)> callback,
    blink::PermissionType permission_type) {
  if (!guest_->attached()) {
    std::move(callback).Run(content::PermissionResult(
        blink::mojom::PermissionStatus::DENIED,
        content::PermissionStatusSource::UNSPECIFIED));
    return;
  }
  guest_->browser_context()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          guest_->embedder_rfh(),
          content::PermissionRequestDescription(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(permission_type),
              user_gesture),
          std::move(callback));
}

}  // namespace guest_view
