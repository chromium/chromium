// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/digital_asset_links/browser_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/digital_asset_links/digital_asset_links_constants.h"
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

void BrowserURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // TODO(crbug.com/1376958): Check the headers in |response_head| for CSP.

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(delegate_);

  // Do the verification here, as redirected urls are not verified, only the
  // final url.
  *defer = true;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OriginVerificationSchedulerBridge::Verify, base::Unretained(bridge_),
          response_url.spec(),
          base::BindOnce(&BrowserURLLoaderThrottle::OnCompleteCheck,
                         weak_factory_.GetWeakPtr(), response_url.spec())));
}

void BrowserURLLoaderThrottle::OnCompleteCheck(std::string url, bool verified) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(delegate_);

  if (verified) {
    delegate_->Resume();
  } else {
    // TODO(crbug.com/1376958): Show an interstitial for blocked content.
    delegate_->CancelWithError(kNetErrorCodeForDigitalAssetLinks,
                               kCustomCancelReasonForURLLoader);
  }
}

const char* BrowserURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "DigitalAssetLinksBrowserThrottle";
}

}  // namespace digital_asset_links
