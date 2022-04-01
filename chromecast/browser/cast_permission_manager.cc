// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_permission_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_permission_user_data.h"
#include "chromecast/common/activity_url_filter.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsRequestingOriginAllowed(
    const GURL& requesting_origin,
    const GURL& app_web_url,
    const std::vector<std::string>& additional_feature_permission_origins) {
  // |app_web_url| is allowed by default.
  if (requesting_origin == app_web_url.DeprecatedGetOriginAsURL()) {
    return true;
  }
  chromecast::ActivityUrlFilter activity_url_filter(
      additional_feature_permission_origins);
  return activity_url_filter.UrlMatchesWhitelist(requesting_origin);
}

blink::mojom::PermissionStatus GetPermissionStatusFromCastPermissionUserData(
    content::PermissionType permission,
    const GURL& requesting_origin,
    chromecast::shell::CastPermissionUserData* cast_permission_user_data) {
  std::string app_id = cast_permission_user_data->GetAppId();
  GURL app_web_url = cast_permission_user_data->GetAppWebUrl();
  // We expect to grant content::PermissionType::PROTECTED_MEDIA_IDENTIFIER
  // to origins same as |app_web_url| by default.
  if (requesting_origin != app_web_url.DeprecatedGetOriginAsURL() ||
      permission != content::PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
    chromecast::metrics::CastMetricsHelper::GetInstance()
        ->RecordApplicationEventWithValue(
            app_id, /*session_id=*/"", /*sdk_version=*/"",
            "Cast.Platform.PermissionRequestWithFrame",
            static_cast<int>(permission));
  }

  if (!cast_permission_user_data->GetEnforceFeaturePermissions()) {
    return blink::mojom::PermissionStatus::GRANTED;
  }

  // Permissions that are granted by default should have been added to the
  // FeaturePermissions in CastPermissionUserData.
  bool permitted =
      cast_permission_user_data->GetFeaturePermissions().count(
          static_cast<int32_t>(permission)) > 0 &&
      IsRequestingOriginAllowed(
          requesting_origin, app_web_url,
          cast_permission_user_data->GetAdditionalFeaturePermissionOrigins());

  return permitted ? blink::mojom::PermissionStatus::GRANTED
                   : ::blink::mojom::PermissionStatus::DENIED;
}

}  // namespace

namespace chromecast {
namespace shell {

// TODO(b/191718807): Be more restrictive on the permissions.
// Currently, only collect metrics.
blink::mojom::PermissionStatus GetPermissionStatusInternal(
    content::PermissionType permission,
    const GURL& requesting_origin) {
  // We expect to grant content::PermissionType::BACKGROUND_SYNC by
  // default.
  if (permission != content::PermissionType::BACKGROUND_SYNC) {
    metrics::CastMetricsHelper::GetInstance()->RecordApplicationEventWithValue(
        "Cast.Platform.PermissionRequestWithoutFrame",
        static_cast<int>(permission));
  }
  blink::mojom::PermissionStatus permission_status =
      blink::mojom::PermissionStatus::GRANTED;
  LOG(INFO) << __func__ << ": "
            << (permission_status == blink::mojom::PermissionStatus::GRANTED
                    ? " grants "
                    : " doesn't grant ")
            << "permission " << static_cast<int>(permission)
            << " out of frame context.";
  return permission_status;
}

blink::mojom::PermissionStatus GetPermissionStatusInternal(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  DCHECK(render_frame_host);
  CastPermissionUserData* cast_permission_user_data =
      CastPermissionUserData::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));

  if (!cast_permission_user_data) {
    LOG(ERROR) << __func__ << ": No permission data in frame!";
    return GetPermissionStatusInternal(permission, requesting_origin);
  }

  blink::mojom::PermissionStatus permission_status =
      GetPermissionStatusFromCastPermissionUserData(
          permission, requesting_origin, cast_permission_user_data);
  LOG(INFO) << __func__ << ": "
            << (permission_status == blink::mojom::PermissionStatus::GRANTED
                    ? " grants "
                    : " doesn't grant ")
            << "permission " << static_cast<int>(permission)
            << " to frame associated with app: "
            << cast_permission_user_data->GetAppId();
  return permission_status;
}

CastPermissionManager::CastPermissionManager() {}

CastPermissionManager::~CastPermissionManager() {}

void CastPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  blink::mojom::PermissionStatus permission_status =
      GetPermissionStatusInternal(permission, render_frame_host,
                                  requesting_origin);
  std::move(callback).Run(permission_status);
}

void CastPermissionManager::RequestPermissions(
    const std::vector<content::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  std::vector<blink::mojom::PermissionStatus> permission_statuses;
  for (auto permission : permissions) {
    permission_statuses.push_back(GetPermissionStatusInternal(
        permission, render_frame_host, requesting_origin));
  }
  std::move(callback).Run(permission_statuses);
}

void CastPermissionManager::ResetPermission(content::PermissionType permission,
                                            const GURL& requesting_origin,
                                            const GURL& embedding_origin) {}

blink::mojom::PermissionStatus CastPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return GetPermissionStatusInternal(permission, requesting_origin);
}

blink::mojom::PermissionStatus
CastPermissionManager::GetPermissionStatusForFrame(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  return GetPermissionStatusInternal(permission, render_frame_host,
                                     requesting_origin);
}

blink::mojom::PermissionStatus
CastPermissionManager::GetPermissionStatusForCurrentDocument(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host) {
  return GetPermissionStatusInternal(
      permission, render_frame_host,
      render_frame_host->GetLastCommittedOrigin().GetURL());
}

blink::mojom::PermissionStatus
CastPermissionManager::GetPermissionStatusForWorker(
    content::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return GetPermissionStatusInternal(permission, worker_origin);
}

CastPermissionManager::SubscriptionId
CastPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return SubscriptionId();
}

void CastPermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}

}  // namespace shell
}  // namespace chromecast
