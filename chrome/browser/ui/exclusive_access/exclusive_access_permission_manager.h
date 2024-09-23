// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_PERMISSION_MANAGER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_PERMISSION_MANAGER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {
class PermissionController;
}  // namespace content

class ExclusiveAccessContext;

// Forwards permission requests for keyboard lock and pointer lock to
// PermissionController, and keeps track of outstanding permission requests.
class ExclusiveAccessPermissionManager : public content::WebContentsObserver {
 public:
  explicit ExclusiveAccessPermissionManager(
      ExclusiveAccessContext* exclusive_access_context);
  ~ExclusiveAccessPermissionManager() override;
  ExclusiveAccessPermissionManager(const ExclusiveAccessPermissionManager&) =
      delete;
  ExclusiveAccessPermissionManager& operator=(
      const ExclusiveAccessPermissionManager&) = delete;

  // If there are other requests from `web_contents` that have already been made
  // and are waiting for a response from PermissionController, then the new
  // request is made immediately. Otherwise we wait for a short moment so that
  // multiple requests made in close succession can be grouped together, then
  // make the request(s).
  void QueuePermissionRequest(blink::PermissionType permission_type,
                              base::OnceClosure granted_callback,
                              base::OnceClosure denied_callback,
                              content::WebContents* web_contents);

  void set_permission_controller_for_test(
      raw_ptr<content::PermissionController> permission_controller) {
    permission_controller_for_test_ = permission_controller;
  }

 private:
  struct PermissionRequest {
   public:
    PermissionRequest(blink::PermissionType type,
                      base::OnceClosure granted_callback,
                      base::OnceClosure denied_callback);
    PermissionRequest(PermissionRequest&& other);
    ~PermissionRequest();

    blink::PermissionType type;
    base::OnceClosure granted_callback;
    base::OnceClosure denied_callback;
  };

  // Per-WebContents grouping of requests.
  struct PendingRequests {
   public:
    PendingRequests();
    ~PendingRequests();

    // Requests that we haven't submitted to PermissionController yet. We
    // gather requests for a short period of time so that multiple requests made
    // in close succession can be grouped together and be presented to the user
    // in a single permission prompt.
    std::vector<PermissionRequest> pending;
    // The number of requests we've made and are waiting for a response from
    // PermissionController. `pending.size() + waiting_responses`
    // is the total number of outstanding requests.
    size_t waiting_responses = 0;
    // We enqueue callbacks here when PermissionController tells us the request
    // results one by one. Once the size of this vector reaches
    // `waiting_responses`, we call all the callbacks at once.
    std::vector<base::OnceClosure> result_callbacks;
  };

  void RequestPermissions(content::GlobalRenderFrameHostId rfh_id,
                          base::WeakPtr<content::WebContents> web_contents);

  void HandleRequestResult(
      content::GlobalRenderFrameHostId rfh_id,
      base::WeakPtr<content::WebContents> web_contents,
      base::OnceClosure granted_callback,
      base::OnceClosure denied_callback,
      const std::vector<blink::mojom::PermissionStatus>& status);

  content::PermissionController* GetPermissionController(
      content::WebContents* web_contents);

  raw_ptr<content::PermissionController> permission_controller_for_test_ =
      nullptr;

  std::map<content::GlobalRenderFrameHostId, PendingRequests> pending_requests_;
  const raw_ptr<ExclusiveAccessContext> exclusive_access_context_;
  base::WeakPtrFactory<ExclusiveAccessPermissionManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_PERMISSION_MANAGER_H_
