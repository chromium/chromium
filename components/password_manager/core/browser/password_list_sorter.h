// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LIST_SORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LIST_SORTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/types/strong_alias.h"

namespace password_manager {

struct PasswordForm;

// Multimap from sort key to password forms.
using DuplicatesMap = std::multimap<std::string, std::unique_ptr<PasswordForm>>;

// Sort entries of |list| based on sort key. The key is the concatenation of
// origin, entry type (non-Android credential, Android w/ affiliated web realm
// or Android w/o affiliated web realm). If a form in |list| is not blocklisted,
// username, password and federation are also included in sort key. Forms that
// only differ by password_form::PasswordForm::Store are merged. If there are
// several forms with the same key, all such forms but the first one are stored
// in |duplicates| instead of |list|.
void SortEntriesAndHideDuplicates(
    std::vector<std::unique_ptr<PasswordForm>>* list,
    DuplicatesMap* duplicates);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LIST_SORTER_H_
