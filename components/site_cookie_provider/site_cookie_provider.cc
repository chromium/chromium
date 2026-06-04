// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_cookie_provider/site_cookie_provider.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace site_cookie_provider {

SiteCookieProvider::~SiteCookieProvider() = default;

class SiteCookieProviderImpl : public SiteCookieProvider {
 public:
  SiteCookieProviderImpl(
      signin::IdentityManager* identity_manager,
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : identity_manager_(identity_manager),
        cookie_manager_(std::move(cookie_manager)),
        url_loader_factory_(url_loader_factory) {}
  ~SiteCookieProviderImpl() override = default;

  // SiteCookieProvider:
  void UpdateState() override {
    // TODO(crbug.com/494306001): Implement state synchronization
    // updates.
  }

 private:
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// static
std::unique_ptr<SiteCookieProvider> SiteCookieProvider::Create(
    signin::IdentityManager* identity_manager,
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<SiteCookieProviderImpl>(
      identity_manager, std::move(cookie_manager), url_loader_factory);
}

}  // namespace site_cookie_provider
