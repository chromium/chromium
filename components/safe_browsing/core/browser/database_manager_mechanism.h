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
      CheckBrowseUrlType check_type,
      bool check_allowlist);

  DatabaseManagerMechanism(const DatabaseManagerMechanism&) = delete;
  DatabaseManagerMechanism& operator=(const DatabaseManagerMechanism&) = delete;
  ~DatabaseManagerMechanism() override;

 private:
  // SafeBrowsingLookupMechanism implementation:
  StartCheckResult StartCheckInternal() override;

  // Callback from the allowlist check. If |did_match_allowlist| is true, this
  // will skip the blocklist check. Should only be called if `check_allowlist_`
  // is true.
  void OnCheckUrlForHighConfidenceAllowlist(
      bool did_match_allowlist,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  // Calls |CheckBrowseUrl| on the database manager and sets
  // is_async_blocklist_check_in_progress_ if the check is asynchronous.
  StartCheckResult StartBlocklistCheck();

  // Similar to |StartBlocklistCheck|, but delivers the result through
  // the callback.
  void StartBlocklistCheckAfterAllowlistCheck();

  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override;

  ThreatSource GetThreatSource() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // If the allowlist should be checked first before checking the blocklist.
  const bool check_allowlist_;

  // Tracks whether there is currently an async blocklist check call into the
  // database manager that the checker is waiting to hear back on. This is used
  // in order to decide whether to ask the database manager to cancel the check
  // on destruct.
  // We don't need to check the progress of allowlist check, because
  // `database_manager_` doesn't require explicit cancelling if the callback is
  // not handled by SafeBrowsingDatabaseManager::Client.
  bool is_async_blocklist_check_in_progress_ = false;

  // The type of check that is passed into |CheckBrowseUrl| on the
  // database manager.
  CheckBrowseUrlType check_type_;

  base::WeakPtrFactory<DatabaseManagerMechanism> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DATABASE_MANAGER_MECHANISM_H_
