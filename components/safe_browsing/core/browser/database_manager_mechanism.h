// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DATABASE_MANAGER_MECHANISM_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DATABASE_MANAGER_MECHANISM_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "url/gurl.h"

namespace safe_browsing {

class V5GetHashProtocolManager;

// This performs Safe Browsing checks using the database manager.
class DatabaseManagerMechanism : public SafeBrowsingLookupMechanism,
                                 public SafeBrowsingDatabaseManager::Client {
 public:
  DatabaseManagerMechanism(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      CheckBrowseUrlType check_type,
      base::WeakPtr<V5GetHashProtocolManager> v5_get_hash_protocol_manager,
      scoped_refptr<base::SequencedTaskRunner> custom_callback_task_runner);

  DatabaseManagerMechanism(const DatabaseManagerMechanism&) = delete;
  DatabaseManagerMechanism& operator=(const DatabaseManagerMechanism&) = delete;
  ~DatabaseManagerMechanism() override;

 private:
  // SafeBrowsingLookupMechanism implementation:
  StartCheckResult StartCheckInternal() override;

  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type) override;
  base::WeakPtr<V5GetHashProtocolManager> GetV5GetHashProtocolManager()
      override;
  scoped_refptr<base::SequencedTaskRunner> GetCustomCallbackTaskRunner()
      override;

  ThreatSource GetThreatSource() const;

  // Logs the `threat_type`from the result of the URL check.
  void LogCheckResult(SBThreatType threat_type);

  SEQUENCE_CHECKER(sequence_checker_);

  // Tracks whether there is currently an async blocklist check call into the
  // database manager that the checker is waiting to hear back on. This is used
  // in order to decide whether to ask the database manager to cancel the check
  // on destruct.
  bool is_async_blocklist_check_in_progress_ = false;

  // The type of check that is passed into |CheckBrowseUrl| on the
  // database manager.
  CheckBrowseUrlType check_type_;

  // The protocol manager used for Safe Browsing v5 get hash requests.
  base::WeakPtr<V5GetHashProtocolManager> v5_get_hash_protocol_manager_;

  // Optional UI-thread task runner used to dispatch asynchronous local database
  // lookup replies for this mechanism.
  scoped_refptr<base::SequencedTaskRunner> custom_callback_task_runner_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DATABASE_MANAGER_MECHANISM_H_
