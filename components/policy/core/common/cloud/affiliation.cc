// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/affiliation.h"

#include "components/policy/core/common/device_local_account_type.h"

namespace policy {

bool IsAffiliated(const base::flat_set<std::string>& user_ids,
                  const base::flat_set<std::string>& device_ids) {
  for (const std::string& device_id : device_ids) {
    if (user_ids.count(device_id))
      return true;
  }
  return false;
}

bool IsUserAffiliated(const base::flat_set<std::string>& user_affiliation_ids,
                      const base::flat_set<std::string>& device_affiliation_ids,
                      base::StringPiece email) {
  // An empty username means incognito user in case of Chrome OS and no
  // logged-in user in case of Chrome (SigninService). Many tests use nonsense
  // email addresses (e.g. 'test') so treat those as non-enterprise users.
  if (email.empty() || email.find('@') == base::StringPiece::npos) {
    return false;
  }

  if (IsDeviceLocalAccountUser(email)) {
    return true;
  }

  return IsAffiliated(user_affiliation_ids, device_affiliation_ids);
}

}  // namespace policy
