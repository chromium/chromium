// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_discount_infos_storage.h"

namespace commerce {

MockDiscountInfosStorage::MockDiscountInfosStorage()
    : DiscountInfosStorage(nullptr, nullptr) {
}
MockDiscountInfosStorage::~MockDiscountInfosStorage() = default;

}  // namespace commerce
