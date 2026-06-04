// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_H_
#define COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace site_cookie_provider {

// Interface for the core logic of managing site-specific cookies.
class SiteCookieProvider {
 public:
  static std::unique_ptr<SiteCookieProvider> Create(
      signin::IdentityManager* identity_manager,
      mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~SiteCookieProvider();

  // Triggers local state synchronization updates.
  virtual void UpdateState() = 0;
};

}  // namespace site_cookie_provider

#endif  // COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_H_
