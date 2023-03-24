// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_INCLUDE_EXCLUDE_ACCOUNT_ID_FILTER_H_
#define COMPONENTS_USER_MANAGER_INCLUDE_EXCLUDE_ACCOUNT_ID_FILTER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager_export.h"

namespace user_manager {

// IncludeExcludeAccountIdFilter is a data structure which helps to identify
// whether `AccountId` is included or excluded from some set (e.g. set of
// ephemeral accounts).
//
// It holds include set, exclude set and a boolean flag which indicates
// whether item is included by default if not present neither in include set
// nor in exclude set.
//
// For example, every account is ephemeral or not, so we can have a set of
// ephemeral accounts and appropriate IncludeExcludeAccountIdFilter if we
// provide next things:
//   1. list of accounts that are ephemeral (include_list)
//   2. list of accounts that are non-ephemeral (exclude_list)
//   3. boolean flag (included_by_default) which configures whether we consider
//      account ephemeral (included to the set) if account is neither present in
//      include nor in exclude lists.
class USER_MANAGER_EXPORT IncludeExcludeAccountIdFilter {
 public:
  IncludeExcludeAccountIdFilter();
  IncludeExcludeAccountIdFilter(bool included_by_default,
                                std::vector<AccountId> include_list,
                                std::vector<AccountId> exclude_list);

  IncludeExcludeAccountIdFilter(IncludeExcludeAccountIdFilter&& other);
  IncludeExcludeAccountIdFilter& operator=(
      IncludeExcludeAccountIdFilter&& other);

  ~IncludeExcludeAccountIdFilter();

  // Returns whether |item| is included by following next rules:
  //   1. Returns true if |item| is present in |include_set_|;
  //   2. Returns false if |item| is present in |exclude_set_|;
  //   3. Returns value of |included_by_default_|.
  bool IsAccountIdIncluded(const AccountId& item) const;

 private:
  bool included_by_default_ = false;
  base::flat_set<AccountId> include_set_;
  base::flat_set<AccountId> exclude_set_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_INCLUDE_EXCLUDE_ACCOUNT_ID_FILTER_H_
