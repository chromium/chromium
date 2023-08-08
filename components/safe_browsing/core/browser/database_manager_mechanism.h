// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DATABASE_MANAGER_MECHANISM_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DATABASE_MANAGER_MECHANISM_H_

#include "base/sequence_checker.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "url/gurl.h"

namespace safe_browsing {

// This performs Safe Browsing checks using the database manager.
class DatabaseManagerMechanism : public SafeBrowsingLookupMechanism,
                                 public SafeBrowsingDatabaseManager::Client {
 public:
  DatabaseManagerMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      MechanismExperimentHashDatabaseCache experiment_cache_selection,
      CheckBrowseUrlType check_type);

  DatabaseManagerMechanism(const DatabaseManagerMechanism&) = delete;
  DatabaseManagerMechanism& operator=(const DatabaseManagerMechanism&) = delete;
  ~DatabaseManagerMechanism() override;

 private:
  // SafeBrowsingLookupMechanism implementation:
  // Calls |CheckBrowseUrl| on the database manager and sets
  // is_async_database_manager_check_in_progress_ if the check is asynchronous.
  StartCheckResult StartCheckInternal() override;

  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Tracks whether there is currently an async call into the database manager
  // that the checker is waiting to hear back on. This is used in order to
  // decide whether to ask the database manager to cancel the check on destruct.
  bool is_async_database_manager_check_in_progress_ = false;

  // The type of check that is passed into |CheckBrowseUrl| on the
  // database manager.
  CheckBrowseUrlType check_type_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DATABASE_MANAGER_MECHANISM_H_
