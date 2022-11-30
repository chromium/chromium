// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_TEST_CHROMEDRIVER_NET_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContext;
}

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit URLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  URLRequestContextGetter(const URLRequestContextGetter&) = delete;
  URLRequestContextGetter& operator=(const URLRequestContextGetter&) = delete;

  // Overridden from net::URLRequestContextGetter:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 private:
  ~URLRequestContextGetter() override;

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Only accessed on the IO thread.
  std::unique_ptr<net::URLRequestContext> url_request_context_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_URL_REQUEST_CONTEXT_GETTER_H_
