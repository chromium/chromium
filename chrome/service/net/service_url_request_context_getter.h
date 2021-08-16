// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>
#include <string>

#include "net/cookies/cookie_monster.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class ProxyConfigService;
}

class ServiceURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  void set_user_agent(const std::string& ua) {
    user_agent_ = ua;
  }
  std::string user_agent() const {
    return user_agent_;
  }

 private:
  friend class ServiceProcess;
  ServiceURLRequestContextGetter();
  ~ServiceURLRequestContextGetter() override;

  std::string user_agent_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
};

#endif  // CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_GETTER_H_
