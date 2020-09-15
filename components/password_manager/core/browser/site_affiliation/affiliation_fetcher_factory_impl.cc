// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/affiliation_fetcher_factory_impl.h"

#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

AffiliationFetcherFactoryImpl::AffiliationFetcherFactoryImpl() = default;
AffiliationFetcherFactoryImpl::~AffiliationFetcherFactoryImpl() = default;

std::unique_ptr<AffiliationFetcherInterface>
AffiliationFetcherFactoryImpl::CreateInstance(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate) {
  return std::make_unique<AffiliationFetcher>(std::move(url_loader_factory),
                                              delegate);
}

}  // namespace password_manager
