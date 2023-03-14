// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/geolocation_permission_context_system.h"

#include "components/content_settings/core/browser/content_settings_observer.h"
#include "content/public/browser/browser_thread.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"

namespace permissions {
GeolocationPermissionContextSystem::GeolocationPermissionContextSystem(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate,
    device::GeolocationManager* geolocation_manager)
    : GeolocationPermissionContext(browser_context, std::move(delegate)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(geolocation_manager);
  system_permission_observers_ = geolocation_manager->GetObserverList();
  system_permission_observers_->AddObserver(this);
  system_permission_ = geolocation_manager->GetSystemPermission();
}

GeolocationPermissionContextSystem::~GeolocationPermissionContextSystem() {
  system_permission_observers_->RemoveObserver(this);
}

ContentSetting GeolocationPermissionContextSystem::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  auto site_permission =
      GeolocationPermissionContext::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);
  if (site_permission != ContentSetting::CONTENT_SETTING_ALLOW) {
    return site_permission;
  }

  switch (system_permission_) {
    case LocationSystemPermissionStatus::kNotDetermined:
      return ContentSetting::CONTENT_SETTING_ASK;
    case LocationSystemPermissionStatus::kDenied:
      return ContentSetting::CONTENT_SETTING_BLOCK;
    case LocationSystemPermissionStatus::kAllowed:
      return ContentSetting::CONTENT_SETTING_ALLOW;
  }
}

void GeolocationPermissionContextSystem::OnSystemPermissionUpdated(
    LocationSystemPermissionStatus new_status) {
  system_permission_ = new_status;
  for (permissions::Observer& obs : permission_observers_) {
    obs.OnPermissionChanged(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsTypeSet(ContentSettingsType::GEOLOCATION));
  }
}

}  // namespace permissions
