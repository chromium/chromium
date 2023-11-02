// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_SCOPED_MOCK_OFFLINE_CONTENT_PROVIDER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_SCOPED_MOCK_OFFLINE_CONTENT_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"

namespace offline_items_collection {

class OfflineContentAggregator;
class OfflineContentProvider;

class ScopedMockOfflineContentProvider : public MockOfflineContentProvider {
 public:
  class ScopedMockObserver : public MockObserver {
   public:
    explicit ScopedMockObserver(OfflineContentProvider* provider);
    ScopedMockObserver();
    ~ScopedMockObserver() override;

    void AddProvider(OfflineContentProvider* provider);

   private:
    raw_ptr<OfflineContentProvider> provider_;
  };

  ScopedMockOfflineContentProvider(const std::string& name_space,
                                   OfflineContentAggregator* aggregator);
  ~ScopedMockOfflineContentProvider() override;

 protected:
  void Unregister();

 private:
  const std::string name_space_;
  raw_ptr<OfflineContentAggregator> aggregator_;
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_TEST_SUPPORT_SCOPED_MOCK_OFFLINE_CONTENT_PROVIDER_H_
