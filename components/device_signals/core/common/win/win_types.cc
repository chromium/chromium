// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/win/win_types.h"

#include "base/values.h"

namespace device_signals {

bool AvProduct::operator==(const AvProduct& other) const {
  return display_name == other.display_name && state == other.state &&
         product_id == other.product_id;
}

base::Value AvProduct::ToValue() const {
  base::Value::Dict values;
  values.Set("displayName", display_name);
  values.Set("state", static_cast<int>(state));
  values.Set("productId", product_id);
  return base::Value(std::move(values));
}

bool InstalledHotfix::operator==(const InstalledHotfix& other) const {
  return hotfix_id == other.hotfix_id;
}

base::Value InstalledHotfix::ToValue() const {
  base::Value::Dict values;
  values.Set("hotfixId", hotfix_id);
  return base::Value(std::move(values));
}

}  // namespace device_signals
