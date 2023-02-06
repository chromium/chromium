// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_DATABASE_MECHANISM_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_DATABASE_MECHANISM_H_

#include "base/sequence_checker.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "url/gurl.h"

namespace safe_browsing {

// This performs the hash-based Safe Browsing check using the database manager.
class HashDatabaseMechanism : public SafeBrowsingLookupMechanism,
                              public SafeBrowsingDatabaseManager::Client {
 public:
  HashDatabaseMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      bool can_check_db,
      MechanismExperimentHashDatabaseCache experiment_cache_selection);

  HashDatabaseMechanism(const HashDatabaseMechanism&) = delete;
  HashDatabaseMechanism& operator=(const HashDatabaseMechanism&) = delete;
  ~HashDatabaseMechanism() override;

 private:
  // SafeBrowsingLookupMechanism implementation:
  StartCheckResult StartCheckInternal() override;

  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override;

  // Calls |CheckBrowseUrl| on the database manager and sets
  // is_async_database_manager_check_in_progress_ if the check is asynchronous.
  bool CallCheckBrowseUrl();

  SEQUENCE_CHECKER(sequence_checker_);

  // Tracks whether there is currently an async call into the database manager
  // that the checker is waiting to hear back on. This is used in order to
  // decide whether to ask the database manager to cancel the check on destruct.
  bool is_async_database_manager_check_in_progress_ = false;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASH_DATABASE_MECHANISM_H_
