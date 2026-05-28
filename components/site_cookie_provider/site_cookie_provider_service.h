// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_H_
#define COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace site_cookie_provider {

class SiteCookieProvider;

// A KeyedService that manages the lifecycle of the SiteCookieProvider.
class SiteCookieProviderService : public KeyedService {
 public:
  SiteCookieProviderService(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~SiteCookieProviderService() override;

  SiteCookieProviderService(const SiteCookieProviderService&) = delete;
  SiteCookieProviderService& operator=(const SiteCookieProviderService&) =
      delete;

  // Triggers local state synchronization updates.
  void UpdateState();

 private:
  std::unique_ptr<SiteCookieProvider> provider_;
};

}  // namespace site_cookie_provider

#endif  // COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_H_
