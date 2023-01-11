// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_NO_STATE_PREFETCH_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/no_state_prefetch/common/prerender_canceler.mojom.h"
#include "net/base/request_priority.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace prerender {

class PrerenderURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public base::SupportsWeakPtr<PrerenderURLLoaderThrottle> {
 public:
  PrerenderURLLoaderThrottle(
      const std::string& histogram_prefix,
      mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler);
  ~PrerenderURLLoaderThrottle() override;

  // Called when the prerender is used. This will unpaused requests and set the
  // priorities to the original value.
  void PrerenderUsed();

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
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

  void OnTimedOut();

  std::string histogram_prefix_;

  bool deferred_ = false;
  int redirect_count_ = 0;
  network::mojom::RequestDestination request_destination_;

  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler_;

  // The throttle changes most request priorities to IDLE during prerendering.
  // The priority is reset back to the original priority when prerendering is
  // finished.
  absl::optional<net::RequestPriority> original_request_priority_;

  base::OnceClosure destruction_closure_;

  base::OneShotTimer detached_timer_;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_
