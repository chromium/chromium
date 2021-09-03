// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync/driver/sync_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

class AffiliationService;

// Activates affiliation-based matching for |password_store|, The
// AffiliationService will use |url_loader_factory| to fetch affiliation
// information.
void EnableAffiliationBasedMatching(PasswordStore* password_store,
                                    AffiliationService* affiliation_service);

// Creates a LoginDatabase. Looks in |profile_path| for the database file.
// Does not call LoginDatabase::Init() -- to avoid UI jank, that needs to be
// called by PasswordStore::Init() on the background thread.
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForProfileStorage(
    const base::FilePath& profile_path);
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForAccountStorage(
    const base::FilePath& profile_path);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_
