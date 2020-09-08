// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/test/url_request_intercepting_job_factory.h"

#include <utility>

#include "base/check_op.h"
#include "net/url_request/url_request_interceptor.h"

namespace cronet {

URLRequestInterceptingJobFactory::URLRequestInterceptingJobFactory(
    net::URLRequestJobFactory* job_factory,
    net::URLRequestInterceptor* interceptor)
    : job_factory_(job_factory), interceptor_(interceptor) {}

URLRequestInterceptingJobFactory::~URLRequestInterceptingJobFactory() = default;

net::URLRequestJob* URLRequestInterceptingJobFactory::CreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  net::URLRequestJob* job =
      interceptor_->MaybeInterceptRequest(request, network_delegate);
  if (job)
    return job;
  return job_factory_->CreateJob(request, network_delegate);
}

bool URLRequestInterceptingJobFactory::IsSafeRedirectTarget(
    const GURL& location) const {
  return job_factory_->IsSafeRedirectTarget(location);
}

}  // namespace cronet
