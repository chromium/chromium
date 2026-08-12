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

void AndroidNativeIdpFetcher::Fetch(
    const content::NativeIdpFetcher::RequestParams& params,
    FetchCallback callback) {
  if (pending_callback_) {
    std::move(callback).Run(base::unexpected(FetchError::kFetchFailed));
    return;
  }
  pending_callback_ = std::move(callback);

  StartRequest(params);
}

void AndroidNativeIdpFetcher::StartRequest(
    content::NativeIdpFetcher::RequestParams params) {
  if (idp_service_) {
    DispatchFetchRequest(params);
    return;
  }

  resolver_->Resolve(
      idp_origin_,
      base::BindOnce(&AndroidNativeIdpFetcher::OnOriginResolved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void AndroidNativeIdpFetcher::DispatchFetchRequest(
    const content::NativeIdpFetcher::RequestParams& params) {
  idp_service_->Fetch(params.url.spec(), params.body, params.headers,
                      base::BindOnce(&AndroidNativeIdpFetcher::OnFetched,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidNativeIdpFetcher::OnOriginResolved(
    content::NativeIdpFetcher::RequestParams params,
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
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void AndroidNativeIdpFetcher::OnConnected(
    content::NativeIdpFetcher::RequestParams params,
    bool connected) {
  if (!connected) {
    idp_service_.reset();
    if (pending_callback_) {
      std::move(pending_callback_)
          .Run(base::unexpected(FetchError::kConnectionFailed));
    }
    return;
  }

  DispatchFetchRequest(params);
}

void AndroidNativeIdpFetcher::OnFetched(
    const std::optional<std::string>& response) {
  if (!pending_callback_) {
    return;
  }

  if (!response.has_value()) {
    idp_service_.reset();
    std::move(pending_callback_)
        .Run(base::unexpected(FetchError::kFetchFailed));
    return;
  }

  std::move(pending_callback_).Run(base::ok(response.value()));
}

}  // namespace chrome
