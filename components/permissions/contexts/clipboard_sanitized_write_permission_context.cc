// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/clipboard_sanitized_write_permission_context.h"

#include <optional>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

namespace permissions {

ClipboardSanitizedWritePermissionContext::
    ClipboardSanitizedWritePermissionContext(
        content::BrowserContext* browser_context,
        std::unique_ptr<ClipboardPermissionContextDelegate> delegate)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::CLIPBOARD_SANITIZED_WRITE,
          network::mojom::PermissionsPolicyFeature::kClipboardWrite),
      delegate_(std::move(delegate)) {}

ClipboardSanitizedWritePermissionContext::
    ~ClipboardSanitizedWritePermissionContext() = default;

void ClipboardSanitizedWritePermissionContext::DecidePermission(
    std::unique_ptr<PermissionRequestData> request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto [callback_delegate, callback_base] =
      base::SplitOnceCallback(std::move(callback));

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  CHECK(delegate_);
  if (delegate_->DecidePermission(*request_data,
                                  std::move(callback_delegate))) {
    return;
  }
#endif

  ContentSettingPermissionContextBase::DecidePermission(
      std::move(request_data), std::move(callback_base));
}

ContentSetting
ClipboardSanitizedWritePermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  CHECK(delegate_);
  if (render_frame_host) {
    if (auto permission_status_delegated =
            delegate_->GetPermissionStatusForWebview(render_frame_host,
                                                     requesting_origin)) {
      return *permission_status_delegated;
    }
  } else {
    // If there is no RenderFrameHost, it might be a worker.
    if (auto permission_status_delegated =
            delegate_->GetPermissionStatusForWorker(browser_context(),
                                                    requesting_origin)) {
      return *permission_status_delegated;
    }
  }
#endif

  // If the delegate returns std::nullopt (e.g. for non-extension origins),
  // fall through to CONTENT_SETTING_ALLOW. This mirrors frame-based behavior
  // where sanitized clipboard write is allowed by default.
  return CONTENT_SETTING_ALLOW;
}

}  // namespace permissions
