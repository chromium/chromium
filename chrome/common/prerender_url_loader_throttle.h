// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/common/prerender.mojom.h"
#include "chrome/common/prerender_types.h"
#include "content/public/common/resource_type.h"
#include "net/base/request_priority.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace prerender {

class PrerenderURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public base::SupportsWeakPtr<PrerenderURLLoaderThrottle> {
 public:
  // If the throttle needs to cancel the prerender, it will run
  // |canceler_getter| on |canceler_getter_task_runner| to do so.
  using CancelerGetterCallback =
      base::OnceCallback<chrome::mojom::PrerenderCanceler*()>;
  PrerenderURLLoaderThrottle(
      PrerenderMode mode,
      const std::string& histogram_prefix,
      CancelerGetterCallback canceler_getter,
      scoped_refptr<base::SequencedTaskRunner> canceler_getter_task_runner);
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
  void WillRedirectRequest(net::RedirectInfo* redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           bool* defer,
                           std::vector<std::string>* to_be_removed_headers,
                           net::HttpRequestHeaders* modified_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

  void OnTimedOut();

  PrerenderMode mode_;
  std::string histogram_prefix_;

  bool deferred_ = false;
  bool sync_xhr_ = false;
  int redirect_count_ = 0;
  content::ResourceType resource_type_;

  CancelerGetterCallback canceler_getter_;
  scoped_refptr<base::SequencedTaskRunner> canceler_getter_task_runner_;

  // The throttle changes most request priorities to IDLE during prerendering.
  // The priority is reset back to the original priority when prerendering is
  // finished.
  base::Optional<net::RequestPriority> original_request_priority_;

  base::OnceClosure destruction_closure_;

  base::OneShotTimer detached_timer_;
};

}  // namespace prerender

#endif  // CHROME_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_
