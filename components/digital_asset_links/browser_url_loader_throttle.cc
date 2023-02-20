// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/digital_asset_links/browser_url_loader_throttle.h"

#include "base/android/build_info.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/digital_asset_links/digital_asset_links_constants.h"
#include "components/digital_asset_links/response_header_verifier.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace digital_asset_links {

BrowserURLLoaderThrottle::OriginVerificationSchedulerBridge::
    OriginVerificationSchedulerBridge() = default;
BrowserURLLoaderThrottle::OriginVerificationSchedulerBridge::
    ~OriginVerificationSchedulerBridge() = default;

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::Create(
    OriginVerificationSchedulerBridge* bridge) {
  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(bridge));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    OriginVerificationSchedulerBridge* bridge)
    : bridge_(bridge) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() = default;

bool BrowserURLLoaderThrottle::VerifyHeader(
    const network::mojom::URLResponseHead& response_head) {
  std::string header_value;
  response_head.headers->GetNormalizedHeader(kEmbedderAncestorHeader,
                                             &header_value);
  return digital_asset_links::ResponseHeaderVerifier::Verify(
      base::android::BuildInfo::GetInstance()->host_package_name(),
      header_value);
}

void BrowserURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  url_ = request->url;
}

void BrowserURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  DCHECK(delegate_);

  *defer = true;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OriginVerificationSchedulerBridge::Verify,
                     base::Unretained(bridge_), url_.spec(),
                     base::BindOnce(&BrowserURLLoaderThrottle::OnCompleteCheck,
                                    weak_factory_.GetWeakPtr(), url_.spec(),
                                    VerifyHeader(response_head))));
  url_ = redirect_info->new_url;
}

void BrowserURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(delegate_);

  *defer = true;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OriginVerificationSchedulerBridge::Verify, base::Unretained(bridge_),
          response_url.spec(),
          base::BindOnce(&BrowserURLLoaderThrottle::OnCompleteCheck,
                         weak_factory_.GetWeakPtr(), response_url.spec(),
                         VerifyHeader(*response_head))));
}

void BrowserURLLoaderThrottle::OnCompleteCheck(std::string url,
                                               bool header_verification_result,
                                               bool dal_verified) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(delegate_);

  if (dal_verified || header_verification_result) {
    delegate_->Resume();
    return;
  }
  delegate_->CancelWithError(kNetErrorCodeForDigitalAssetLinks,
                             kCustomCancelReasonForURLLoader);
}

const char* BrowserURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "DigitalAssetLinksBrowserThrottle";
}

}  // namespace digital_asset_links
