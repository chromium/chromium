// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_context_base.h"

namespace webrtc {
class MediaStreamDeviceEnumerator;
}  // namespace webrtc

namespace permissions {

// Manage user permissions that only control camera movement (pan, tilt, and
// zoom). Those permissions are automatically reset when the "regular" camera
// permission is blocked or reset.
class CameraPanTiltZoomPermissionContext
    : public permissions::PermissionContextBase {
 public:
  // Delegate which allows embedders to modify the logic of this permission
  // context.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Allows the delegate to override the context's
    // GetPermissionStatusInternal() logic. If this returns true, the base
    // context's GetPermissionStatusInternal() will not be called.
    virtual bool GetPermissionStatusInternal(
        const GURL& requesting_origin,
        const GURL& embedding_origin,
        ContentSetting* content_setting_result) = 0;
  };

  // Constructs a CameraPanTiltZoomPermissionContext for |browser_context|. Note
  // that the passed in |device_enumerator| must outlive |this|.
  CameraPanTiltZoomPermissionContext(
      content::BrowserContext* browser_context,
      std::unique_ptr<Delegate> delegate,
      const webrtc::MediaStreamDeviceEnumerator* device_enumerator);
  ~CameraPanTiltZoomPermissionContext() override;

  CameraPanTiltZoomPermissionContext(
      const CameraPanTiltZoomPermissionContext&) = delete;
  CameraPanTiltZoomPermissionContext& operator=(
      const CameraPanTiltZoomPermissionContext&) = delete;

 private:
  // PermissionContextBase
  void RequestPermission(
      PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  // content_settings::Observer
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Returns true if at least one video capture device has PTZ capabilities.
  // Otherwise returns false.
  bool HasAvailableCameraPtzDevices() const;

  std::unique_ptr<Delegate> delegate_;

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  bool updating_camera_ptz_permission_ = false;
  bool updating_mediastream_camera_permission_ = false;

  // Enumerates available media devices. Must outlive |this|.
  const raw_ptr<const webrtc::MediaStreamDeviceEnumerator> device_enumerator_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_
