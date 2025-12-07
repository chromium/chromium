// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/clipboard_read_write_permission_context.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

ClipboardReadWritePermissionContext::ClipboardReadWritePermissionContext(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClipboardPermissionContextDelegate> delegate)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::CLIPBOARD_READ_WRITE,
          network::mojom::PermissionsPolicyFeature::kClipboardRead),
      delegate_(std::move(delegate)) {}

ClipboardReadWritePermissionContext::~ClipboardReadWritePermissionContext() =
    default;

void ClipboardReadWritePermissionContext::DecidePermission(
    std::unique_ptr<PermissionRequestData> request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto [callback_delegate, callback_base] =
      base::SplitOnceCallback(std::move(callback));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  CHECK(delegate_);
  if (delegate_->DecidePermission(*request_data,
                                  std::move(callback_delegate))) {
    return;
  }
#endif

  PermissionContextBase::DecidePermission(std::move(request_data),
                                          std::move(callback_base));
}

ContentSetting
ClipboardReadWritePermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  CHECK(delegate_);
  if (auto permission_status_delegated = delegate_->GetPermissionStatus(
          render_frame_host, requesting_origin)) {
    return *permission_status_delegated;
  }
#endif

  return ContentSettingPermissionContextBase::GetContentSettingStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);
}

void ClipboardReadWritePermissionContext::UpdateTabContext(
    const PermissionRequestData& request_data,
    bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          request_data.id.global_render_frame_host_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnContentAllowed(
        ContentSettingsType::CLIPBOARD_READ_WRITE);
  } else {
    content_settings->OnContentBlocked(
        ContentSettingsType::CLIPBOARD_READ_WRITE);
  }
}

}  // namespace permissions
