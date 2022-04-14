// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTER_THROTTLE_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTER_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/url_param_filter/cross_otr_observer.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace url_param_filter {
class UrlParamFilterThrottle : public blink::URLLoaderThrottle {
 public:
  // Create the throttle if conditions warrant doing so, and add it to
  // `throttle_list` if created. Otherwise, leave `throttle_list` unchanged and
  // do nothing.
  static void MaybeCreateThrottle(
      bool enabled_by_policy,
      content::WebContents* web_contents,
      const network::ResourceRequest& request,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttle_list);
  explicit UrlParamFilterThrottle(
      const absl::optional<url::Origin>& request_initiator_origin,
      base::WeakPtr<CrossOtrObserver> observer);
  ~UrlParamFilterThrottle() override;

  UrlParamFilterThrottle(const UrlParamFilterThrottle&) = delete;
  UrlParamFilterThrottle& operator=(const UrlParamFilterThrottle&) = delete;

  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;
  bool makes_unsafe_redirect() override;

 private:
  GURL last_hop_initiator_;
  bool should_filter_;
  base::WeakPtr<CrossOtrObserver> observer_;
};
}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTER_THROTTLE_H_
