// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/url_validity_checker_impl.h"

#include "base/bind.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"

namespace {

// Request timeout duration.
constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(10);

}  // namespace

// Stores the pending request and associated metadata. Deleted once the request
// finishes.
struct UrlValidityCheckerImpl::PendingRequest {
  PendingRequest(const GURL& url,
                 const base::TimeTicks& time_created,
                 const base::TickClock* clock,
                 UrlValidityCheckerCallback callback)
      : url(url),
        time_created(time_created),
        timeout_timer(clock),
        callback(std::move(callback)) {}

  GURL url;
  base::TimeTicks time_created;
  base::OneShotTimer timeout_timer;
  UrlValidityCheckerCallback callback;
  std::unique_ptr<network::SimpleURLLoader> loader;

  DISALLOW_COPY_AND_ASSIGN(PendingRequest);
};

UrlValidityCheckerImpl::UrlValidityCheckerImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::TickClock* tick_clock)
    : url_loader_factory_(std::move(url_loader_factory)), clock_(tick_clock) {}

UrlValidityCheckerImpl::~UrlValidityCheckerImpl() = default;

void UrlValidityCheckerImpl::DoesUrlResolve(
    const GURL& url,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    UrlValidityCheckerCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "HEAD";
  resource_request->allow_credentials = false;

  auto request_iter = pending_requests_.emplace(pending_requests_.begin(), url,
                                                clock_->NowTicks(), clock_,
                                                std::move(callback));
  request_iter->loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Don't follow redirects to prevent leaking URL data to HTTP sites.
  request_iter->loader->SetOnRedirectCallback(
      base::BindRepeating(&UrlValidityCheckerImpl::OnSimpleLoaderRedirect,
                          weak_ptr_factory_.GetWeakPtr(), request_iter));
  request_iter->loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&UrlValidityCheckerImpl::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_iter),
      /*max_body_size=*/1);
  request_iter->timeout_timer.Start(
      FROM_HERE, kRequestTimeout,
      base::BindOnce(&UrlValidityCheckerImpl::OnSimpleLoaderHandler,
                     weak_ptr_factory_.GetWeakPtr(), request_iter, false));
}

void UrlValidityCheckerImpl::OnSimpleLoaderRedirect(
    std::list<PendingRequest>::iterator request_iter,
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  // Assume the URL is valid if a redirect is returned.
  OnSimpleLoaderHandler(request_iter, true);
}

void UrlValidityCheckerImpl::OnSimpleLoaderComplete(
    std::list<PendingRequest>::iterator request_iter,
    std::unique_ptr<std::string> response_body) {
  // |response_body| is null for non-2xx responses.
  OnSimpleLoaderHandler(request_iter, response_body.get() != nullptr);
}

void UrlValidityCheckerImpl::OnSimpleLoaderHandler(
    std::list<PendingRequest>::iterator request_iter,
    bool valid) {
  base::TimeDelta elapsed_time =
      clock_->NowTicks() - request_iter->time_created;
  std::move(request_iter->callback).Run(valid, elapsed_time);
  pending_requests_.erase(request_iter);
}
