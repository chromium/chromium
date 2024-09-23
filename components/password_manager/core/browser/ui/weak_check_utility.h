// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_WEAK_CHECK_UTILITY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_WEAK_CHECK_UTILITY_H_

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/types/strong_alias.h"

namespace password_manager {

using IsWeakPassword = base::StrongAlias<class IsWeakPasswordTag, bool>;

// Returns whether `password` is weak.
IsWeakPassword IsWeak(std::u16string_view password);

// Checks each password for weakness and removes strong passwords from the
// |passwords|.
base::flat_set<std::u16string> BulkWeakCheck(
    base::flat_set<std::u16string> passwords);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_WEAK_CHECK_UTILITY_H_
