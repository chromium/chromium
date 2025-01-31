// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"

#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/webrtc/media_stream_device_enumerator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace permissions {

// TODO(crbug.com/40205763): This method is a temporary solution because of
// inconsistency between the new permissions API that is migrated to
// `blink::mojom::PermissionStatus` and its callsites that still use
// `ContentSetting`.
void CallbackWrapper(base::OnceCallback<void(ContentSetting)> callback,
                     blink::mojom::PermissionStatus status) {
  ContentSetting result = CONTENT_SETTING_ASK;
  if (status == blink::mojom::PermissionStatus::GRANTED) {
    result = CONTENT_SETTING_ALLOW;
  } else if (status == blink::mojom::PermissionStatus::DENIED) {
    result = CONTENT_SETTING_BLOCK;
  }
  std::move(callback).Run(result);
}

CameraPanTiltZoomPermissionContext::CameraPanTiltZoomPermissionContext(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate,
    const webrtc::MediaStreamDeviceEnumerator* device_enumerator)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                            blink::mojom::PermissionsPolicyFeature::kNotFound),
      delegate_(std::move(delegate)),
      device_enumerator_(device_enumerator) {
  DCHECK(device_enumerator_);
  host_content_settings_map_ =
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
  content_setting_observer_registered_by_subclass_ = true;
  host_content_settings_map_->AddObserver(this);
}

CameraPanTiltZoomPermissionContext::~CameraPanTiltZoomPermissionContext() {
  host_content_settings_map_->RemoveObserver(this);
}

void CameraPanTiltZoomPermissionContext::RequestPermission(
    PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (HasAvailableCameraPtzDevices()) {
    PermissionContextBase::RequestPermission(std::move(request_data),
                                             std::move(callback));
    return;
  }

  // If there is no camera with PTZ capabilities, let's request a "regular"
  // camera permission instead.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(
          request_data.id.global_render_frame_host_id());

  if (request_data.requesting_origin !=
      render_frame_host->GetLastCommittedOrigin().GetURL()) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }
  render_frame_host->GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          render_frame_host,
          content::PermissionRequestDescription(
              blink::PermissionType::VIDEO_CAPTURE, request_data.user_gesture),
          base::BindOnce(&CallbackWrapper, std::move(callback)));
}

ContentSetting CameraPanTiltZoomPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  ContentSetting result = CONTENT_SETTING_DEFAULT;
  if (delegate_->GetPermissionStatusInternal(requesting_origin,
                                             embedding_origin, &result)) {
    return result;
  }
  return PermissionContextBase::GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);
}

void CameraPanTiltZoomPermissionContext::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  PermissionContextBase::OnContentSettingChanged(
      primary_pattern, secondary_pattern, content_type_set);

  if (!content_type_set.Contains(ContentSettingsType::MEDIASTREAM_CAMERA) &&
      !content_type_set.Contains(ContentSettingsType::CAMERA_PAN_TILT_ZOOM)) {
    return;
  }

  // Skip if the camera permission is currently being updated to match camera
  // PTZ permission as OnContentSettingChanged would have been called again
  // causing a reentrancy issue.
  if (updating_mediastream_camera_permission_) {
    updating_mediastream_camera_permission_ = false;
    return;
  }

  // Skip if the camera PTZ permission is currently being reset when camera
  // permission got blocked or reset as OnContentSettingChanged would have been
  // called again causing a reentrancy issue.
  if (updating_camera_ptz_permission_) {
    updating_camera_ptz_permission_ = false;
    return;
  }

  // TODO(crbug.com/40129438): We should not need to deduce the url from the
  // primary pattern here. Modify the infrastructure to facilitate this
  // particular use case better.
  const GURL url(primary_pattern.ToString());
  if (url::Origin::Create(url).opaque())
    return;

  ContentSetting camera_ptz_setting =
      host_content_settings_map_->GetContentSetting(url, url,
                                                    content_settings_type());

  if (content_type_set.Contains(ContentSettingsType::CAMERA_PAN_TILT_ZOOM)) {
    // Automatically update camera permission to camera PTZ permission as any
    // change to camera PTZ should be reflected to camera.
    updating_mediastream_camera_permission_ = true;
    host_content_settings_map_->SetContentSettingCustomScope(
        primary_pattern, secondary_pattern,
        ContentSettingsType::MEDIASTREAM_CAMERA, camera_ptz_setting);
    return;
  }

  // Don't reset camera PTZ permission if it is already blocked or in a
  // "default" state.
  if (camera_ptz_setting == CONTENT_SETTING_BLOCK ||
      camera_ptz_setting == CONTENT_SETTING_ASK) {
    return;
  }

  ContentSetting mediastream_camera_setting =
      host_content_settings_map_->GetContentSetting(
          url, url, ContentSettingsType::MEDIASTREAM_CAMERA);
  if (mediastream_camera_setting == CONTENT_SETTING_BLOCK ||
      mediastream_camera_setting == CONTENT_SETTING_ASK) {
    // Automatically reset camera PTZ permission if camera permission
    // gets blocked or reset.
    updating_camera_ptz_permission_ = true;
    host_content_settings_map_->SetContentSettingCustomScope(
        primary_pattern, secondary_pattern,
        ContentSettingsType::CAMERA_PAN_TILT_ZOOM, CONTENT_SETTING_DEFAULT);
  }
}

bool CameraPanTiltZoomPermissionContext::HasAvailableCameraPtzDevices() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::vector<blink::MediaStreamDevice> devices =
      device_enumerator_->GetVideoCaptureDevices();
  for (const blink::MediaStreamDevice& device : devices) {
    if (device.video_control_support.pan || device.video_control_support.tilt ||
        device.video_control_support.zoom) {
      return true;
    }
  }
  return false;
}

}  // namespace permissions
