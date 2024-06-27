// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/url_loader_factory_provider.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "net/cert/cert_verifier.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace enterprise_companion {

namespace {

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit URLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
      : network_task_runner_(network_task_runner),
        proxy_config_service_(
            net::ProxyConfigService::CreateSystemProxyConfigService(
                network_task_runner)) {}

  URLRequestContextGetter(const URLRequestContextGetter&) = delete;
  URLRequestContextGetter& operator=(const URLRequestContextGetter&) = delete;

  // Overrides for net::URLRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override {
    if (!url_request_context_.get()) {
      net::URLRequestContextBuilder builder;
      builder.DisableHttpCache();
      builder.set_proxy_config_service(std::move(proxy_config_service_));
      cert_net_fetcher_ = base::MakeRefCounted<net::CertNetFetcherURLRequest>();
      auto cert_verifier = net::CertVerifier::CreateDefault(cert_net_fetcher_);
      builder.SetCertVerifier(std::move(cert_verifier));
      url_request_context_ = builder.Build();
      cert_net_fetcher_->SetURLRequestContext(url_request_context_.get());
    }
    return url_request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return network_task_runner_;
  }

 protected:
  ~URLRequestContextGetter() override { cert_net_fetcher_->Shutdown(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  scoped_refptr<net::CertNetFetcherURLRequest> cert_net_fetcher_;
};

}  // namespace

URLLoaderFactoryProvider::URLLoaderFactoryProvider()
    : url_loader_factory_owner_(
          base::MakeRefCounted<URLRequestContextGetter>(
              base::SingleThreadTaskRunner::GetCurrentDefault()),
          /*is_trusted=*/true) {}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
URLLoaderFactoryProvider::GetPendingURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_loader_factory_owner_.GetURLLoaderFactory()->Clone();
}

}  // namespace enterprise_companion
