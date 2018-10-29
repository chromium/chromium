// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_URL_VALIDITY_CHECKER_IMPL_H_
#define COMPONENTS_SEARCH_URL_VALIDITY_CHECKER_IMPL_H_

#include <list>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/search/url_validity_checker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
struct ResourceResponseHead;
class SharedURLLoaderFactory;
}  // namespace network

class UrlValidityCheckerImpl : public UrlValidityChecker {
 public:
  UrlValidityCheckerImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::TickClock* tick_clock);
  ~UrlValidityCheckerImpl() override;

  void DoesUrlResolve(const GURL& url,
                      net::NetworkTrafficAnnotationTag traffic_annotation,
                      UrlValidityCheckerCallback callback) override;

 private:
  struct PendingRequest;

  // Called when the request times out. Calls back false and returns the request
  // duration.
  void OnRequestTimeout(std::list<PendingRequest>::iterator request_iter);

  void OnSimpleLoaderRedirect(
      std::list<PendingRequest>::iterator request_iter,
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head,
      std::vector<std::string>* to_be_removed_headers);
  void OnSimpleLoaderComplete(std::list<PendingRequest>::iterator request_iter,
                              std::unique_ptr<std::string> response_body);
  // Called when the request from |DoesUrlResolve| finishes. Invokes the
  // associated callback with the request status and duration.
  void OnSimpleLoaderHandler(std::list<PendingRequest>::iterator request_iter,
                             bool valid);

  // Stores any ongoing network requests. Once a request is completed, it is
  // deleted from the list.
  std::list<PendingRequest> pending_requests_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Non-owned pointer to TickClock. Used for request timeouts.
  const base::TickClock* const clock_;

  base::WeakPtrFactory<UrlValidityCheckerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UrlValidityCheckerImpl);
};

#endif  // COMPONENTS_SEARCH_URL_VALIDITY_CHECKER_IMPL_H_
