// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_permission_manager.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_permission_user_data.h"
#include "chromecast/common/activity_url_filter.h"
#include "components/cast_receiver/browser/public/permissions_manager.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

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
    blink::PermissionType permission,
    const GURL& requesting_origin,
    chromecast::shell::CastPermissionUserData* cast_permission_user_data) {
  std::string app_id = cast_permission_user_data->GetAppId();
  GURL app_web_url = cast_permission_user_data->GetAppWebUrl();
  // We expect to grant blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER
  // to origins same as |app_web_url| by default.
  if (requesting_origin != app_web_url.DeprecatedGetOriginAsURL() ||
      permission != blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
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
    blink::PermissionType permission,
    const GURL& requesting_origin) {
  // We expect to grant blink::PermissionType::BACKGROUND_SYNC by
  // default.
  if (permission != blink::PermissionType::BACKGROUND_SYNC) {
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
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  const cast_receiver::PermissionsManager* permissions_manager =
      cast_receiver::PermissionsManager::GetInstance(*web_contents);
  if (permissions_manager) {
    const blink::mojom::PermissionStatus permission_status =
        permissions_manager->GetPermissionStatus(permission, requesting_origin);
    if (permission_status == blink::mojom::PermissionStatus::GRANTED) {
      return permission_status;
    }
  }

  DCHECK(render_frame_host);
  CastPermissionUserData* cast_permission_user_data =
      CastPermissionUserData::FromWebContents(web_contents);

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

void CastPermissionManager::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  std::vector<blink::mojom::PermissionStatus> permission_statuses;
  for (auto permission : request_description.permissions) {
    permission_statuses.push_back(GetPermissionStatusInternal(
        permission, render_frame_host, request_description.requesting_origin));
  }
  std::move(callback).Run(permission_statuses);
}

void CastPermissionManager::ResetPermission(blink::PermissionType permission,
                                            const GURL& requesting_origin,
                                            const GURL& embedding_origin) {}

void CastPermissionManager::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  std::vector<blink::mojom::PermissionStatus> permission_statuses;
  for (auto permission : request_description.permissions) {
    permission_statuses.push_back(GetPermissionStatusInternal(
        permission, render_frame_host,
        render_frame_host->GetLastCommittedOrigin().GetURL()));
  }
  std::move(callback).Run(permission_statuses);
}

blink::mojom::PermissionStatus CastPermissionManager::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return GetPermissionStatusInternal(permission, requesting_origin);
}

content::PermissionResult
CastPermissionManager::GetPermissionResultForOriginWithoutContext(
    blink::PermissionType permission,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  blink::mojom::PermissionStatus status = GetPermissionStatus(
      permission, requesting_origin.GetURL(), embedding_origin.GetURL());

  return content::PermissionResult(
      status, content::PermissionStatusSource::UNSPECIFIED);
}

blink::mojom::PermissionStatus
CastPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  return GetPermissionStatusInternal(
      permission, render_frame_host,
      render_frame_host->GetLastCommittedOrigin().GetURL());
}

blink::mojom::PermissionStatus
CastPermissionManager::GetPermissionStatusForWorker(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return GetPermissionStatusInternal(permission, worker_origin);
}

blink::mojom::PermissionStatus
CastPermissionManager::GetPermissionStatusForEmbeddedRequester(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& requesting_origin) {
  return GetPermissionStatusInternal(permission, render_frame_host,
                                     requesting_origin.GetURL());
}

}  // namespace shell
}  // namespace chromecast
