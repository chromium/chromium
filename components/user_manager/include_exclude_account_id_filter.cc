// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/include_exclude_account_id_filter.h"

#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "components/account_id/account_id.h"

namespace user_manager {

namespace {

bool AreSetsMutuallyExclusive(const base::flat_set<AccountId>& set1,
                              const base::flat_set<AccountId>& set2) {
  for (const auto& item : set1) {
    if (set2.count(item) > 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

IncludeExcludeAccountIdFilter::IncludeExcludeAccountIdFilter() = default;

IncludeExcludeAccountIdFilter::IncludeExcludeAccountIdFilter(
    bool included_by_default,
    std::vector<AccountId> include_list,
    std::vector<AccountId> exclude_list)
    : included_by_default_(included_by_default),
      include_set_(include_list.begin(), include_list.end()),
      exclude_set_(exclude_list.begin(), exclude_list.end()) {
  DCHECK(AreSetsMutuallyExclusive(include_set_, exclude_set_));
}

IncludeExcludeAccountIdFilter::IncludeExcludeAccountIdFilter(
    IncludeExcludeAccountIdFilter&& other) = default;

IncludeExcludeAccountIdFilter& IncludeExcludeAccountIdFilter::operator=(
    IncludeExcludeAccountIdFilter&& other) = default;

IncludeExcludeAccountIdFilter::~IncludeExcludeAccountIdFilter() = default;

bool IncludeExcludeAccountIdFilter::IsAccountIdIncluded(
    const AccountId& item) const {
  if (include_set_.count(item) > 0) {
    return true;
  }

  if (exclude_set_.count(item) > 0) {
    return false;
  }

  return included_by_default_;
}

}  // namespace user_manager
