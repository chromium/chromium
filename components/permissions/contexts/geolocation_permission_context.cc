// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/geolocation_permission_context.h"

#include <variant>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/resolvers/geolocation_permission_resolver.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/origin.h"

namespace permissions {

GeolocationPermissionContext::GeolocationPermissionContext(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate)
    : PermissionContextBase(
          browser_context,
          content_settings::GeolocationContentSettingsType(),
          network::mojom::PermissionsPolicyFeature::kGeolocation),
      delegate_(std::move(delegate)) {}

GeolocationPermissionContext::~GeolocationPermissionContext() = default;

void GeolocationPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!delegate_->DecidePermission(*request_data, &callback, this)) {
    DCHECK(callback);
    PermissionContextBase::DecidePermission(std::move(request_data),
                                            std::move(callback));
  }
}

base::WeakPtr<GeolocationPermissionContext>
GeolocationPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<PermissionResolver>
GeolocationPermissionContext::CreatePermissionResolver(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor) const {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission)) {
    return std::make_unique<GeolocationPermissionResolver>(
        *permission_descriptor);
  } else {
    return PermissionContextBase::CreatePermissionResolver(
        permission_descriptor);
  }
}

void GeolocationPermissionContext::UpdateTabContext(
    const PermissionRequestData& request_data,
    bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          request_data.id.global_render_frame_host_id());

  // WebContents might not exist (extensions) or no longer exist. In which
  // case, PageSpecificContentSettings will be null.
  if (content_settings) {
    if (allowed) {
      content_settings->OnContentAllowed(content_settings_type());
    } else {
      content_settings->OnContentBlocked(content_settings_type());
    }
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
