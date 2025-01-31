// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/geolocation_permission_context.h"

#include "base/functional/bind.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/origin.h"

namespace permissions {

GeolocationPermissionContext::GeolocationPermissionContext(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::GEOLOCATION,
          blink::mojom::PermissionsPolicyFeature::kGeolocation),
      delegate_(std::move(delegate)) {}

GeolocationPermissionContext::~GeolocationPermissionContext() = default;

void GeolocationPermissionContext::DecidePermission(
    PermissionRequestData request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!delegate_->DecidePermission(
          request_data.id, request_data.requesting_origin,
          request_data.user_gesture, &callback, this)) {
    DCHECK(callback);
    PermissionContextBase::DecidePermission(std::move(request_data),
                                            std::move(callback));
  }
}

base::WeakPtr<GeolocationPermissionContext>
GeolocationPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void GeolocationPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.global_render_frame_host_id());

  // WebContents might not exist (extensions) or no longer exist. In which case,
  // PageSpecificContentSettings will be null.
  if (content_settings) {
    if (allowed)
      content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
    else
      content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
  }

  if (allowed) {
    GetGeolocationControl()->UserDidOptIntoLocationServices();
  }
}

device::mojom::GeolocationControl*
GeolocationPermissionContext::GetGeolocationControl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!geolocation_control_) {
    content::GetDeviceService().BindGeolocationControl(
        geolocation_control_.BindNewPipeAndPassReceiver());
  }
  return geolocation_control_.get();
}

}  // namespace permissions
