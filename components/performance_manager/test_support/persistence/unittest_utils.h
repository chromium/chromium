// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERSISTENCE_UNITTEST_UTILS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERSISTENCE_UNITTEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace testing {

class MockSiteDataImplOnDestroyDelegate
    : public internal::SiteDataImpl::OnDestroyDelegate {
 public:
  MockSiteDataImplOnDestroyDelegate();

  MockSiteDataImplOnDestroyDelegate(const MockSiteDataImplOnDestroyDelegate&) =
      delete;
  MockSiteDataImplOnDestroyDelegate& operator=(
      const MockSiteDataImplOnDestroyDelegate&) = delete;

  ~MockSiteDataImplOnDestroyDelegate();

  MOCK_METHOD(void,
              OnSiteDataImplDestroyed,
              (internal::SiteDataImpl*),
              (override));

  base::WeakPtr<MockSiteDataImplOnDestroyDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSiteDataImplOnDestroyDelegate> weak_factory_{this};
};

// An implementation of a SiteDataStore that doesn't record anything.
class NoopSiteDataStore : public SiteDataStore {
 public:
  NoopSiteDataStore();

  NoopSiteDataStore(const NoopSiteDataStore&) = delete;
  NoopSiteDataStore& operator=(const NoopSiteDataStore&) = delete;

  ~NoopSiteDataStore() override;

  // SiteDataStore:
  void ReadSiteDataFromStore(const url::Origin& origin,
                             ReadSiteDataFromStoreCallback callback) override;
  void WriteSiteDataIntoStore(
      const url::Origin& origin,
      const SiteDataProto& site_characteristic_proto) override;
  void RemoveSiteDataFromStore(
      const std::vector<url::Origin>& site_origins) override;
  void ClearStore() override;
  void GetStoreSize(GetStoreSizeCallback callback) override;
  void SetInitializationCallbackForTesting(base::OnceClosure callback) override;
};

}  // namespace testing
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERSISTENCE_UNITTEST_UTILS_H_
