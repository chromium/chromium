// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_DISCOUNTS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_DISCOUNTS_STORAGE_H_

#include "components/commerce/core/discounts_storage.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace commerce {

class MockDiscountsStorage : public DiscountsStorage {
 public:
  MockDiscountsStorage();
  MockDiscountsStorage(const MockDiscountsStorage&) = delete;
  ~MockDiscountsStorage() override;

  MOCK_METHOD(void,
              HandleServerDiscounts,
              (const GURL& url,
               std::vector<DiscountInfo> server_results,
               DiscountInfoCallback callback),
              (override));
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_DISCOUNTS_STORAGE_H_
