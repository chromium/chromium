// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/product_group.h"

namespace commerce {

ProductGroup::ProductGroup(const std::string& group_id,
                           const std::string& title)
    : group_id(group_id), title(title) {
  creation_time = base::Time::Now();
  update_time = creation_time;
}

ProductGroup::~ProductGroup() = default;

}  // namespace commerce
