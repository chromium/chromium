// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handling_permission_context.h"

#include <string>

#include "chrome/browser/web_applications/components/file_handling_permission_request_impl.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_context_base.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

FileHandlingPermissionContext::FileHandlingPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::FILE_HANDLING,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

FileHandlingPermissionContext::~FileHandlingPermissionContext() = default;

bool FileHandlingPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

std::unique_ptr<permissions::PermissionRequest>
FileHandlingPermissionContext::CreatePermissionRequest(
    const GURL& request_origin,
    ContentSettingsType content_settings_type,
    bool has_gesture,
    content::WebContents* web_contents,
    permissions::PermissionRequestImpl::PermissionDecidedCallback
        permission_decided_callback,
    base::OnceClosure delete_callback) const {
  return std::make_unique<permissions::FileHandlingPermissionRequestImpl>(
      request_origin, content_settings_type, has_gesture, web_contents,
      std::move(permission_decided_callback), std::move(delete_callback));
}

void FileHandlingPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time) {
  // In the default implementation, `is_one_time` actually means "for this
  // session". In the case of file handling, `is_one_time` should correspond to
  // persistence.
  persist = persist && !is_one_time;
  PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, /*is_one_time=*/false);
}
