// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LIST_SORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LIST_SORTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/util/type_safety/strong_alias.h"
#include "components/password_manager/core/browser/password_form_forward.h"

namespace password_manager {

// Multimap from sort key to password forms.
using DuplicatesMap = std::multimap<std::string, std::unique_ptr<PasswordForm>>;
using IgnoreStore = util::StrongAlias<class IgnoreStoreTag, bool>;

// Creates key for sorting password or password exception entries. The key is
// eTLD+1 followed by the reversed list of domains (e.g.
// secure.accounts.example.com => example.com.com.example.accounts.secure) and
// the scheme. If |form| is not blacklisted, username, password and federation
// are appended to the key. If not, no further information is added. For Android
// credentials the canocial spec is included.
// If |ignore_store| is true, forms differing only by the originating password
// store will map to the same key.
std::string CreateSortKey(const PasswordForm& form,
                          IgnoreStore ignore_store = IgnoreStore(false));

// Sort entries of |list| based on sort key. The key is the concatenation of
// origin, entry type (non-Android credential, Android w/ affiliated web realm
// or Android w/o affiliated web realm). If a form in |list| is not blacklisted,
// username, password and federation are also included in sort key. If there are
// several forms with the same key, all such forms but the first one are stored
// in |duplicates| instead of |list|.
void SortEntriesAndHideDuplicates(
    std::vector<std::unique_ptr<PasswordForm>>* list,
    DuplicatesMap* duplicates);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LIST_SORTER_H_
