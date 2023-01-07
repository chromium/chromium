// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/unittest_utils.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "components/performance_manager/performance_manager_impl.h"

namespace performance_manager {
namespace testing {

MockSiteDataImplOnDestroyDelegate::MockSiteDataImplOnDestroyDelegate() =
    default;
MockSiteDataImplOnDestroyDelegate::~MockSiteDataImplOnDestroyDelegate() =
    default;

NoopSiteDataStore::NoopSiteDataStore() = default;
NoopSiteDataStore::~NoopSiteDataStore() = default;

void NoopSiteDataStore::ReadSiteDataFromStore(
    const url::Origin& origin,
    ReadSiteDataFromStoreCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

void NoopSiteDataStore::WriteSiteDataIntoStore(
    const url::Origin& origin,
    const SiteDataProto& site_characteristic_proto) {}

void NoopSiteDataStore::RemoveSiteDataFromStore(
    const std::vector<url::Origin>& site_origins) {}

void NoopSiteDataStore::ClearStore() {}

void NoopSiteDataStore::GetStoreSize(GetStoreSizeCallback callback) {
  std::move(callback).Run(absl::nullopt, absl::nullopt);
}

void NoopSiteDataStore::SetInitializationCallbackForTesting(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

}  // namespace testing
}  // namespace performance_manager
