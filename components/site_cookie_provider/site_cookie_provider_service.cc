// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_cookie_provider/site_cookie_provider_service.h"

#include "base/logging.h"
#include "components/site_cookie_provider/site_cookie_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace site_cookie_provider {

SiteCookieProviderService::SiteCookieProviderService(
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Initialize the site cookie provider.
  provider_ = SiteCookieProvider::Create(std::move(cookie_manager),
                                         std::move(url_loader_factory));
}

SiteCookieProviderService::~SiteCookieProviderService() = default;

void SiteCookieProviderService::UpdateState() {
  if (provider_) {
    provider_->UpdateState();
  }
}

}  // namespace site_cookie_provider
