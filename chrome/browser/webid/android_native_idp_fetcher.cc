// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/android_native_idp_fetcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "chrome/browser/webid/identity_provider_service.h"
#include "chrome/browser/webid/verified_origin_resolver.h"

namespace chrome {

AndroidNativeIdpFetcher::AndroidNativeIdpFetcher(const url::Origin& idp_origin)
    : idp_origin_(idp_origin),
      resolver_(std::make_unique<content::webid::VerifiedOriginResolver>()) {}

AndroidNativeIdpFetcher::~AndroidNativeIdpFetcher() = default;

void AndroidNativeIdpFetcher::FetchAccounts(const GURL& accounts_url,
                                            FetchCallback callback) {
  // TODO(crbug.com/465181345): Support concurrent requests if necessary.
  if (pending_callback_) {
    std::move(callback).Run(base::unexpected(FetchError::kFetchFailed));
    return;
  }
  pending_callback_ = std::move(callback);

  std::string request = accounts_url.spec();

  resolver_->Resolve(idp_origin_,
                     base::BindOnce(&AndroidNativeIdpFetcher::OnOriginResolved,
                                    weak_ptr_factory_.GetWeakPtr(), request));
}

void AndroidNativeIdpFetcher::OnOriginResolved(
    const std::string& request,
    const content::webid::VerifiedOriginResolver::Result& result) {
  if (!result.has_value()) {
    if (pending_callback_) {
      std::move(pending_callback_)
          .Run(base::unexpected(FetchError::kNoServiceFound));
    }
    return;
  }

  idp_service_ = std::make_unique<content::webid::IdentityProviderService>();
  idp_service_->Connect(
      result.value().first, result.value().second,
      base::BindOnce(&AndroidNativeIdpFetcher::OnConnected,
                     weak_ptr_factory_.GetWeakPtr(), request));
}

void AndroidNativeIdpFetcher::OnConnected(const std::string& request,
                                          bool connected) {
  if (!connected) {
    idp_service_.reset();
    if (pending_callback_) {
      std::move(pending_callback_)
          .Run(base::unexpected(FetchError::kConnectionFailed));
    }
    return;
  }

  idp_service_->Fetch(request,
                      base::BindOnce(&AndroidNativeIdpFetcher::OnFetched,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidNativeIdpFetcher::OnFetched(
    const std::optional<std::string>& response) {
  idp_service_.reset();

  if (!pending_callback_) {
    return;
  }

  if (!response.has_value()) {
    std::move(pending_callback_)
        .Run(base::unexpected(FetchError::kFetchFailed));
    return;
  }

  std::move(pending_callback_).Run(base::ok(response.value()));
}

}  // namespace chrome
