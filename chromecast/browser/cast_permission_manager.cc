// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_permission_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"

namespace chromecast {
namespace shell {

CastPermissionManager::CastPermissionManager() {}

CastPermissionManager::~CastPermissionManager() {
}

int CastPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  LOG(INFO) << __FUNCTION__ << ": " << static_cast<int>(permission);
  std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
  return content::PermissionController::kNoPendingOperation;
}

int CastPermissionManager::RequestPermissions(
    const std::vector<content::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
      permissions.size(), blink::mojom::PermissionStatus::GRANTED));
  return content::PermissionController::kNoPendingOperation;
}

void CastPermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

blink::mojom::PermissionStatus CastPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  LOG(INFO) << __FUNCTION__ << ": " << static_cast<int>(permission);
  return blink::mojom::PermissionStatus::GRANTED;
}

blink::mojom::PermissionStatus
CastPermissionManager::GetPermissionStatusForFrame(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  LOG(INFO) << __FUNCTION__ << ": " << static_cast<int>(permission);
  return blink::mojom::PermissionStatus::GRANTED;
}

int CastPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return content::PermissionController::kNoPendingOperation;
}

void CastPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}

}  // namespace shell
}  // namespace chromecast
