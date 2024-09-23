// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"

#include "base/functional/callback_helpers.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"

namespace commerce {

MockProductSpecificationsService::MockProductSpecificationsService()
    : ProductSpecificationsService(
          base::DoNothing(),
          std::make_unique<
              testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>>()) {}

MockProductSpecificationsService::~MockProductSpecificationsService() = default;

}  // namespace commerce
