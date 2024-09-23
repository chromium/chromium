// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/product_group.h"

namespace commerce {

ProductGroup::ProductGroup(const base::Uuid& uuid,
                           const std::string& name,
                           const std::vector<GURL>& urls,
                           base::Time update_time)
    : uuid(uuid),
      name(name),
      member_products(std::set<GURL>(urls.begin(), urls.end())),
      update_time(update_time) {}

ProductGroup::~ProductGroup() = default;

ProductGroup::ProductGroup(const ProductGroup&) = default;

ProductGroup& ProductGroup::operator=(const ProductGroup&) = default;

}  // namespace commerce
