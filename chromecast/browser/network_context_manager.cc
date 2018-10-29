// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/network_context_manager.h"

#include <string>

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"

namespace chromecast {

NetworkContextManager::NetworkContextManager(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : NetworkContextManager(std::move(url_request_context_getter), nullptr) {}

NetworkContextManager::NetworkContextManager(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    std::unique_ptr<network::NetworkService> network_service)
    : url_request_context_getter_(std::move(url_request_context_getter)),
      network_service_for_test_(std::move(network_service)),
      weak_factory_(this) {
  DCHECK(url_request_context_getter_);
  weak_ptr_ = weak_factory_.GetWeakPtr();

  // The NetworkContext must be initialized on the browser's IO thread. Posting
  // this task from the constructor ensures that |network_context_| will
  // be initialized for subsequent calls to BindRequestOnIOThread().
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&NetworkContextManager::InitializeOnIOThread,
                     GetWeakPtr()));
}

NetworkContextManager::~NetworkContextManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void NetworkContextManager::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  network::NetworkService* network_service =
      network_service_for_test_ ? network_service_for_test_.get()
                                : content::GetNetworkServiceImpl();
  network_context_ = std::make_unique<network::NetworkContext>(
      network_service, mojo::MakeRequest(&network_context_ptr_),
      url_request_context_getter_->GetURLRequestContext());
}

void NetworkContextManager::BindRequestOnIOThread(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_corb_enabled = false;
  network_context_->CreateURLLoaderFactory(
      std::move(request), std::move(url_loader_factory_params));
}

void NetworkContextManager::GetProxyResolvingSocketFactoryOnIOThread(
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  network_context_->CreateProxyResolvingSocketFactory(std::move(request));
}

network::mojom::URLLoaderFactoryPtr
NetworkContextManager::GetURLLoaderFactory() {
  network::mojom::URLLoaderFactoryPtr loader_factory;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&NetworkContextManager::BindRequestOnIOThread,
                     GetWeakPtr(), mojo::MakeRequest(&loader_factory)));
  return loader_factory;
}

void NetworkContextManager::GetProxyResolvingSocketFactory(
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &NetworkContextManager::GetProxyResolvingSocketFactoryOnIOThread,
          GetWeakPtr(), std::move(request)));
}

//  static
NetworkContextManager* NetworkContextManager::CreateForTest(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    std::unique_ptr<network::NetworkService> network_service) {
  return new NetworkContextManager(std::move(url_request_context_getter),
                                   std::move(network_service));
}

}  // namespace chromecast
