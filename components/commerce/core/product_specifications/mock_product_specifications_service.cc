// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"

namespace commerce {

// static
std::unique_ptr<KeyedService> MockProductSpecificationsService::Build() {
  return std::make_unique<
      testing::NiceMock<MockProductSpecificationsService>>();
}

MockProductSpecificationsService::MockProductSpecificationsService() = default;

MockProductSpecificationsService::~MockProductSpecificationsService() = default;

}  // namespace commerce
