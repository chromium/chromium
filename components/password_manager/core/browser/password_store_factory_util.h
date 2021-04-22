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

namespace network {
class NetworkConnectionTracker;
}  // namespace network

namespace password_manager {

// Activates or deactivates affiliation-based matching for |password_store|,
// depending on whether or not the |sync_service| is syncing passwords stored
// therein. The AffiliationService will use |url_loader_factory| to fetch
// affiliation information. This function should be called whenever there is a
// possibility that syncing passwords has just started or ended.
void ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
    PasswordStore* password_store,
    syncer::SyncService* sync_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::FilePath& profile_path);

// Creates a LoginDatabase. Looks in |profile_path| for the database file.
// Does not call LoginDatabase::Init() -- to avoid UI jank, that needs to be
// called by PasswordStore::Init() on the background thread.
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForProfileStorage(
    const base::FilePath& profile_path);
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForAccountStorage(
    const base::FilePath& profile_path);

// Deletes any files associated with the acccount-scoped LoginDatabase. Do *not*
// call this while a LoginDatabase instance is using these files!
void DeleteLoginDatabaseForAccountStorageFiles(
    const base::FilePath& profile_path);

base::FilePath GetLoginDatabaseForAccountStoragePathForTesting(
    const base::FilePath& profile_path);

// Determines if affiliation based matching can be performed.
// It checks whether the user
// 1) has password syncing across multiple devices enabled
//    (first setup must be completed)
// 2) does not have explicit passphrase set.
// Failure to meet both of those requirements results in preventing Chrome from
// sending requests to Google Affiliation Service API.
bool ShouldAffiliationBasedMatchingBeActive(syncer::SyncService* sync_service);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_
