// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/affiliation.h"

namespace policy {

bool IsAffiliated(const base::flat_set<std::string>& user_ids,
                  const base::flat_set<std::string>& device_ids) {
  for (const std::string& device_id : device_ids) {
    if (user_ids.count(device_id))
      return true;
  }
  return false;
}

}  // namespace policy
