// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_DISCOUNT_INFOS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_DISCOUNT_INFOS_STORAGE_H_

#include "components/commerce/core/discount_infos_storage.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace commerce {

class MockDiscountInfosStorage : public DiscountInfosStorage {
 public:
  MockDiscountInfosStorage();
  MockDiscountInfosStorage(const MockDiscountInfosStorage&) = delete;
  ~MockDiscountInfosStorage() override;

  MOCK_METHOD(void,
              LoadDiscountsWithPrefix,
              (const GURL& url, DiscountInfoCallback callback),
              (override));

  MOCK_METHOD(void,
              SaveDiscounts,
              (const GURL& url, const std::vector<DiscountInfo>& infos),
              (override));
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_DISCOUNT_INFOS_STORAGE_H_
