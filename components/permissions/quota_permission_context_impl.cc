// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/quota_permission_context_impl.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
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

#if defined(OS_ANDROID)
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
      const GURL& origin_url,
      bool is_large_quota_request,
      content::QuotaPermissionContext::PermissionCallback callback);

  ~QuotaPermissionRequest() override;

 private:
  // PermissionRequest:
  IconId GetIconId() const override;
#if defined(OS_ANDROID)
  base::string16 GetMessageText() const override;
#endif
  base::string16 GetMessageTextFragment() const override;
  GURL GetOrigin() const override;
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  PermissionRequestType GetPermissionRequestType() const override;

  const scoped_refptr<QuotaPermissionContextImpl> context_;
  const GURL origin_url_;
  const bool is_large_quota_request_;
  content::QuotaPermissionContext::PermissionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(QuotaPermissionRequest);
};

QuotaPermissionRequest::QuotaPermissionRequest(
    QuotaPermissionContextImpl* context,
    const GURL& origin_url,
    bool is_large_quota_request,
    content::QuotaPermissionContext::PermissionCallback callback)
    : context_(context),
      origin_url_(origin_url),
      is_large_quota_request_(is_large_quota_request),
      callback_(std::move(callback)) {
  // Suppress unused private field warning on desktop.
  ALLOW_UNUSED_LOCAL(is_large_quota_request_);
}

QuotaPermissionRequest::~QuotaPermissionRequest() {}

PermissionRequest::IconId QuotaPermissionRequest::GetIconId() const {
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_FOLDER;
#else
  return vector_icons::kFolderIcon;
#endif
}

#if defined(OS_ANDROID)
base::string16 QuotaPermissionRequest::GetMessageText() const {
  // If the site requested larger quota than this threshold, show a different
  // message to the user.
  return l10n_util::GetStringFUTF16(
      (is_large_quota_request_ ? IDS_REQUEST_LARGE_QUOTA_INFOBAR_TEXT
                               : IDS_REQUEST_QUOTA_INFOBAR_TEXT),
      url_formatter::FormatUrlForSecurityDisplay(origin_url_));
}
#endif

base::string16 QuotaPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_REQUEST_QUOTA_PERMISSION_FRAGMENT);
}

GURL QuotaPermissionRequest::GetOrigin() const {
  return origin_url_;
}

void QuotaPermissionRequest::PermissionGranted() {
  context_->DispatchCallbackOnIOThread(
      std::move(callback_),
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW);
}

void QuotaPermissionRequest::PermissionDenied() {
  context_->DispatchCallbackOnIOThread(
      std::move(callback_),
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_DISALLOW);
}

void QuotaPermissionRequest::Cancelled() {}

void QuotaPermissionRequest::RequestFinished() {
  if (callback_) {
    context_->DispatchCallbackOnIOThread(
        std::move(callback_),
        content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_CANCELLED);
  }

  delete this;
}

PermissionRequestType QuotaPermissionRequest::GetPermissionRequestType() const {
  return PermissionRequestType::QUOTA;
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
