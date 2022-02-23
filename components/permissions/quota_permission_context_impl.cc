// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/quota_permission_context_impl.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/resources/android/theme_resources.h"
#else
#include "components/vector_icons/vector_icons.h"
#endif

namespace permissions {
namespace {

// On Android, if the site requested larger quota than this threshold, show a
// different message to the user.
const int64_t kRequestLargeQuotaThreshold = 5 * 1024 * 1024;

// QuotaPermissionRequest ---------------------------------------------

class QuotaPermissionRequest : public PermissionRequest {
 public:
  QuotaPermissionRequest(
      QuotaPermissionContextImpl* context,
      const GURL& requesting_origin,
      bool is_large_quota_request,
      content::QuotaPermissionContext::PermissionCallback callback);

  QuotaPermissionRequest(const QuotaPermissionRequest&) = delete;
  QuotaPermissionRequest& operator=(const QuotaPermissionRequest&) = delete;

  ~QuotaPermissionRequest() override;

  // PermissionRequest:
  bool IsDuplicateOf(PermissionRequest* other_request) const override;
#if BUILDFLAG(IS_ANDROID)
  std::u16string GetDialogMessageText() const override;
#endif

 private:
  void PermissionDecided(ContentSetting result, bool is_one_time);
  void DeleteRequest();

  const scoped_refptr<QuotaPermissionContextImpl> context_;
  const bool is_large_quota_request_;
  content::QuotaPermissionContext::PermissionCallback callback_;
};

QuotaPermissionRequest::QuotaPermissionRequest(
    QuotaPermissionContextImpl* context,
    const GURL& requesting_origin,
    bool is_large_quota_request,
    content::QuotaPermissionContext::PermissionCallback callback)
    : PermissionRequest(
          requesting_origin,
          permissions::RequestType::kDiskQuota,
          /*has_gesture=*/false,
          base::BindOnce(&QuotaPermissionRequest::PermissionDecided,
                         base::Unretained(this)),
          base::BindOnce(&QuotaPermissionRequest::DeleteRequest,
                         base::Unretained(this))),
      context_(context),
      is_large_quota_request_(is_large_quota_request),
      callback_(std::move(callback)) {}

QuotaPermissionRequest::~QuotaPermissionRequest() {}

bool QuotaPermissionRequest::IsDuplicateOf(
    PermissionRequest* other_request) const {
  // The downcast here is safe because PermissionRequest::IsDuplicateOf ensures
  // that both requests are of type kDiskQuota.
  return permissions::PermissionRequest::IsDuplicateOf(other_request) &&
         is_large_quota_request_ ==
             static_cast<QuotaPermissionRequest*>(other_request)
                 ->is_large_quota_request_;
}

#if BUILDFLAG(IS_ANDROID)
std::u16string QuotaPermissionRequest::GetDialogMessageText() const {
  // If the site requested larger quota than this threshold, show a different
  // message to the user.
  return l10n_util::GetStringFUTF16(
      (is_large_quota_request_ ? IDS_REQUEST_LARGE_QUOTA_INFOBAR_TEXT
                               : IDS_REQUEST_QUOTA_INFOBAR_TEXT),
      url_formatter::FormatUrlForSecurityDisplay(requesting_origin()));
}
#endif  // BUILDFLAG(IS_ANDROID)

void QuotaPermissionRequest::PermissionDecided(ContentSetting result,
                                               bool is_one_time) {
  DCHECK(!is_one_time);
  if (result == CONTENT_SETTING_DEFAULT) {
    // Handled by `DeleteRequest`.
    return;
  }
  auto response =
      result == ContentSetting::CONTENT_SETTING_ALLOW
          ? content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW
          : content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_DISALLOW;
  context_->DispatchCallbackOnIOThread(std::move(callback_), response);
}

void QuotaPermissionRequest::DeleteRequest() {
  if (callback_) {
    context_->DispatchCallbackOnIOThread(
        std::move(callback_),
        content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_CANCELLED);
  }

  delete this;
}

}  // namespace

// QuotaPermissionContextImpl -----------------------------------------------

QuotaPermissionContextImpl::QuotaPermissionContextImpl() {}

void QuotaPermissionContextImpl::RequestQuotaPermission(
    const content::StorageQuotaParams& params,
    int render_process_id,
    PermissionCallback callback) {
  if (params.storage_type != blink::mojom::StorageType::kPersistent) {
    // For now we only support requesting quota with this interface
    // for Persistent storage type.
    std::move(callback).Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    return;
  }

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaPermissionContextImpl::RequestQuotaPermission,
                       this, params, render_process_id, std::move(callback)));
    return;
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id,
                                       params.render_frame_id);
  if (!render_frame_host) {
    // The tab may have gone away or the request may not be from a tab.
    LOG(WARNING) << "Attempt to request quota tabless renderer: "
                 << render_process_id << "," << params.render_frame_id;
    DispatchCallbackOnIOThread(std::move(callback),
                               QUOTA_PERMISSION_RESPONSE_CANCELLED);
    return;
  }

  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  if (permission_request_manager) {
    bool is_large_quota_request =
        params.requested_size > kRequestLargeQuotaThreshold;
    permission_request_manager->AddRequest(
        render_frame_host, new QuotaPermissionRequest(this, params.origin_url,
                                                      is_large_quota_request,
                                                      std::move(callback)));
    return;
  }

  // The tab has no UI service for presenting the permissions request.
  LOG(WARNING) << "Attempt to request quota from a background page: "
               << render_process_id << "," << params.render_frame_id;
  DispatchCallbackOnIOThread(std::move(callback),
                             QUOTA_PERMISSION_RESPONSE_CANCELLED);
}

void QuotaPermissionContextImpl::DispatchCallbackOnIOThread(
    PermissionCallback callback,
    QuotaPermissionResponse response) {
  DCHECK(callback);

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaPermissionContextImpl::DispatchCallbackOnIOThread,
                       this, std::move(callback), response));
    return;
  }

  std::move(callback).Run(response);
}

QuotaPermissionContextImpl::~QuotaPermissionContextImpl() {}

}  // namespace permissions
