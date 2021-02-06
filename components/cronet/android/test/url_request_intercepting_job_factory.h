// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_TEST_URL_REQUEST_INTERCEPTING_JOB_FACTORY_H_
#define COMPONENTS_CRONET_ANDROID_TEST_URL_REQUEST_INTERCEPTING_JOB_FACTORY_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;

namespace net {
class URLRequest;
class URLRequestJob;
class URLRequestInterceptor;
}  // namespace net

namespace cronet {

// This class acts as a wrapper for URLRequestJobFactory.  The
// URLRequestInteceptor is given the option of creating a URLRequestJob for each
// URLRequest. If the interceptor does not create a job, the URLRequest is
// forwarded to the wrapped URLRequestJobFactory instead.
//
// This class is only intended for use in intercepting requests before they
// are passed on to their default ProtocolHandler.  Each supported scheme should
// have its own ProtocolHandler.
class URLRequestInterceptingJobFactory : public net::URLRequestJobFactory {
 public:
  // Does not take ownership of |job_factory| and |interceptor|.
  URLRequestInterceptingJobFactory(net::URLRequestJobFactory* job_factory,
                                   net::URLRequestInterceptor* interceptor);
  ~URLRequestInterceptingJobFactory() override;

  // URLRequestJobFactory implementation
  std::unique_ptr<net::URLRequestJob> CreateJob(
      net::URLRequest* request) const override;
  bool IsSafeRedirectTarget(const GURL& location) const override;

 private:
  net::URLRequestJobFactory* const job_factory_;
  net::URLRequestInterceptor* const interceptor_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestInterceptingJobFactory);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_TEST_URL_REQUEST_INTERCEPTING_JOB_FACTORY_H_
