// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_

#include <string>

#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace desks_storage {

namespace desk_template_util {

// Returns a copy of a duplicated name to be stored.  This function works by
// taking the name to be duplicated and adding a "(1)" to it. If the name
// already has "(1)" then the number inside of the parenthesis will be
// incremented.
std::u16string AppendDuplicateNumberToDuplicateName(
    const std::u16string& duplicate_name_u16);

// Populates the given cache with test app information.
void PopulateAppRegistryCache(AccountId account_id,
                              apps::AppRegistryCache* cache);

void AddAppIdToAppRegistryCache(AccountId account_id,
                                apps::AppRegistryCache* cache,
                                const char* app_id);

}  // namespace desk_template_util

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_
