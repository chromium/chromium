// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/test/url_request_intercepting_job_factory.h"

#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"

namespace cronet {

URLRequestInterceptingJobFactory::URLRequestInterceptingJobFactory(
    net::URLRequestJobFactory* job_factory,
    net::URLRequestInterceptor* interceptor)
    : job_factory_(job_factory), interceptor_(interceptor) {}

URLRequestInterceptingJobFactory::~URLRequestInterceptingJobFactory() = default;

std::unique_ptr<net::URLRequestJob> URLRequestInterceptingJobFactory::CreateJob(
    net::URLRequest* request) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::unique_ptr<net::URLRequestJob> job =
      interceptor_->MaybeInterceptRequest(request);
  if (job)
    return job;
  return job_factory_->CreateJob(request);
}

bool URLRequestInterceptingJobFactory::IsSafeRedirectTarget(
    const GURL& location) const {
  return job_factory_->IsSafeRedirectTarget(location);
}

}  // namespace cronet
