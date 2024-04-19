// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/product_group.h"

namespace commerce {

ProductGroup::ProductGroup(const base::Uuid& uuid,
                           const std::vector<GURL>& urls)
    : uuid(uuid), member_products(std::set<GURL>(urls.begin(), urls.end())) {}

ProductGroup::~ProductGroup() = default;

}  // namespace commerce
