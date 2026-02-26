// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PERMISSION_HELPER_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PERMISSION_HELPER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base {
class DictValue;
}  // namespace base

namespace blink {
enum class PermissionType;
}  // namespace blink

namespace content {
struct PermissionResult;
class RenderFrameHost;
}  // namespace content

namespace guest_view {

namespace mojom {
enum class PageHandler_PermissionResponseAction;
}

class SlimWebViewGuest;

enum class SlimWebViewPermissionType {
  kGeolocation,
  kMedia,
};

class SlimWebViewPermissionHelper {
 public:
  using PermissionResponseAction = mojom::PageHandler_PermissionResponseAction;

  static SlimWebViewPermissionHelper* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  enum class SetPermissionResult {
    kInvalid,
    kAllowed,
    kDenied,
  };

  using PermissionResponseCallback = base::OnceCallback<void(bool)>;

  explicit SlimWebViewPermissionHelper(SlimWebViewGuest* guest);
  ~SlimWebViewPermissionHelper();

  void RequestGeolocationPermission(
      const GURL& requesting_frame_url,
      bool user_gesture,
      base::OnceCallback<void(content::PermissionResult)> callback);
  void RequestMediaAccessPermission(const content::MediaStreamRequest& request,
                                    content::MediaResponseCallback callback);
  void RequestPermission(SlimWebViewPermissionType permission_type,
                         base::DictValue request_info,
                         PermissionResponseCallback callback,
                         bool allowed_by_default);
  SetPermissionResult SetPermission(int request_id,
                                    PermissionResponseAction action);

 private:
  struct PermissionResponseInfo;
  using RequestMap = absl::flat_hash_map<int, PermissionResponseInfo>;

  void OnGeolocationPermissionResponse(
      bool user_gesture,
      base::OnceCallback<void(content::PermissionResult)> callback,
      bool allow);
  void OnMediaPermissionResponse(const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 bool allow);
  void RequestEmbedderFramePermission(
      bool user_gesture,
      base::OnceCallback<void(content::PermissionResult)> callback,
      blink::PermissionType permission_type);

  int next_request_id_ = kInstanceIDNone;
  RequestMap pending_requests_;
  const raw_ptr<SlimWebViewGuest> guest_;

  base::WeakPtrFactory<SlimWebViewPermissionHelper> weak_factory_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PERMISSION_HELPER_H_
