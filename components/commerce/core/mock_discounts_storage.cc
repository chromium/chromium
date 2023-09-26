// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_discounts_storage.h"

namespace commerce {

MockDiscountsStorage::MockDiscountsStorage()
    : DiscountsStorage(nullptr, nullptr) {
  ON_CALL(*this, HandleServerDiscounts)
      .WillByDefault([](const std::vector<std::string>& urls_to_check,
                        DiscountsMap server_results,
                        DiscountInfoCallback callback) {
        std::move(callback).Run(std::move(server_results));
      });
}
MockDiscountsStorage::~MockDiscountsStorage() = default;

}  // namespace commerce
