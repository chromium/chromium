// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_util.h"

namespace password_manager {

PasswordStoreChangeList JoinPasswordStoreChanges(
    std::vector<PasswordStoreChangeList> changes) {
  PasswordStoreChangeList joined_changes;
  for (auto changes_list : changes) {
    std::move(changes_list.begin(), changes_list.end(),
              std::back_inserter(joined_changes));
  }
  return joined_changes;
}

}  // namespace password_manager
