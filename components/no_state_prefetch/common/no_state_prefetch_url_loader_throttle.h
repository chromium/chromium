// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_URL_LOADER_THROTTLE_H_
#define COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_URL_LOADER_THROTTLE_H_

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "components/no_state_prefetch/common/prerender_canceler.mojom.h"
#include "net/base/request_priority.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace prerender {

class NoStatePrefetchURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  NoStatePrefetchURLLoaderThrottle(
      mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler);
  ~NoStatePrefetchURLLoaderThrottle() override;

  void set_destruction_closure(base::OnceClosure closure) {
    destruction_closure_ = std::move(closure);
  }

 private:
  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  const char* NameForLoggingWillStartRequest() override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;

  void OnTimedOut();

  bool deferred_ = false;
  network::mojom::RequestDestination request_destination_;

  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler_;

  base::OnceClosure destruction_closure_;

  base::OneShotTimer detached_timer_;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_COMMON_NO_STATE_PREFETCH_URL_LOADER_THROTTLE_H_
