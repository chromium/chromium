// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_cookie_provider/site_cookie_provider.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace site_cookie_provider {

SiteCookieProvider::~SiteCookieProvider() = default;

class SiteCookieProviderImpl : public SiteCookieProvider {
 public:
  SiteCookieProviderImpl(
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {}

  ~SiteCookieProviderImpl() override = default;

  // SiteCookieProvider:
  void UpdateState() override {
    // TODO(crbug.com/494306001): Implement state synchronization
    // updates.
  }
};

// static
std::unique_ptr<SiteCookieProvider> SiteCookieProvider::Create(
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<SiteCookieProviderImpl>(
      std::move(cookie_manager), std::move(url_loader_factory));
}

}  // namespace site_cookie_provider
