// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_REUSE_CHECK_UTILITY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_REUSE_CHECK_UTILITY_H_

#include <string>

#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/ui/affiliated_group.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace password_manager {

// Returns reused passwords. Password is considered reused only if:
// * there are at least two credentials with the same non-normalized password
//   and those credentials:
//    # have different normalized usernames,
//    # aren't affiliated and/or PSL-matched,
//    # don't belong to internal network.
// TODO(crbug.com/40252723): Refactor the code to accept only 'groups' after
// password grouping is fully adopted.
absl::flat_hash_set<PasswordString> BulkReuseCheck(
    const std::vector<CredentialUIEntry>& credentials,
    const std::vector<AffiliatedGroup>& groups);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_REUSE_CHECK_UTILITY_H_
