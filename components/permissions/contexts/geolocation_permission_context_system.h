// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_SYSTEM_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_SYSTEM_H_

// The flow for some operation systems needs to take into account
// the system geolocation settings so it differs from the other platforms. It
// works as follows.
// GeolocationPermissionContextSystem::RequestPermission intercepts the flow
// and proceeds to check the system location permission.
// If enabled, it proceeds with the per site flow via
// GeolocationPermissionContext (which will check per site permissions, create
// infobars, etc.).
//
// It also fires OnPermissionChanged when the effective permission state may
// have changed when the system permission is updated.

#include "components/permissions/contexts//geolocation_permission_context.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

namespace permissions {

using device::LocationSystemPermissionStatus;

class GeolocationPermissionContextSystem
    : public GeolocationPermissionContext,
      public device::GeolocationSystemPermissionManager::PermissionObserver {
 public:
  GeolocationPermissionContextSystem(content::BrowserContext* browser_context,
                                     std::unique_ptr<Delegate> delegate);
  ~GeolocationPermissionContextSystem() override;

  GeolocationPermissionContextSystem(
      const GeolocationPermissionContextSystem&) = delete;
  GeolocationPermissionContextSystem& operator=(
      const GeolocationPermissionContextSystem&) = delete;

 private:
  // GeolocationPermissionContext:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  // device::GeolocationSystemPermissionManager::PermissionObserver:
  void OnSystemPermissionUpdated(
      LocationSystemPermissionStatus new_status) override;

  LocationSystemPermissionStatus system_permission_ =
      LocationSystemPermissionStatus::kNotDetermined;

  // Must be the last member, to ensure that it will be destroyed first, which
  // will invalidate weak pointers.
  base::WeakPtrFactory<GeolocationPermissionContextSystem> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_GEOLOCATION_PERMISSION_CONTEXT_SYSTEM_H_
