// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_service.h"

namespace commerce {

ProductSpecificationsService::ProductSpecificationsService(
    std::unique_ptr<ProductSpecificationsSyncBridge> bridge)
    : bridge_(std::move(bridge)) {}

ProductSpecificationsService::~ProductSpecificationsService() = default;

}  // namespace commerce
