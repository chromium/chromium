// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/url_request_context_getter.h"

#include <memory>
#include <string>

#include "base/single_thread_task_runner.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

URLRequestContextGetter::URLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : network_task_runner_(network_task_runner) {
}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext() {
  CHECK(network_task_runner_->BelongsToCurrentThread());
  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;
    // net::HttpServer fails to parse headers if user-agent header is blank.
    builder.set_user_agent(base::ToLowerASCII(kChromeDriverProductShortName));
    builder.DisableHttpCache();
    builder.set_proxy_config_service(
        std::make_unique<net::ProxyConfigServiceFixed>(
            net::ProxyConfigWithAnnotation::CreateDirect()));
    url_request_context_ = builder.Build();
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
    URLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

URLRequestContextGetter::~URLRequestContextGetter() {}
