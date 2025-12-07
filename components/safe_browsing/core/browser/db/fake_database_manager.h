// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_FAKE_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_FAKE_DATABASE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "url/gurl.h"

namespace safe_browsing {

// A test support class used to classify given URLs as dangerous so that
// features may test their interaction with Safe Browsing.
class FakeSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  explicit FakeSafeBrowsingDatabaseManager(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  void AddDangerousUrl(const GURL& dangerous_url, SBThreatType threat_type);
  void AddDangerousUrlPattern(const GURL& dangerous_url,
                              ThreatPatternType pattern_type);
  void ClearDangerousUrl(const GURL& dangerous_url);
  void SetHighConfidenceAllowlistMatchResult(const GURL& url,
                                             bool match_allowlist);

  // TestSafeBrowsingDatabaseManager implementation:
  // These are implemented as needed to return stubbed values.
  bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      CheckBrowseUrlType check_type) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      CheckUrlForHighConfidenceAllowlistCallback callback) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  safe_browsing::ThreatSource GetBrowseUrlThreatSource(
      CheckBrowseUrlType check_type) const override;
  safe_browsing::ThreatSource GetNonBrowseUrlThreatSource() const override;

 private:
  ~FakeSafeBrowsingDatabaseManager() override;

  static void CheckBrowseURLAsync(GURL url,
                                  SBThreatType result_threat_type,
                                  ThreatPatternType pattern_type,
                                  Client* client);
  static void CheckDownloadURLAsync(const std::vector<GURL>& url_chain,
                                    SBThreatType result_threat_type,
                                    Client* client);

  base::flat_map<GURL, SBThreatType> dangerous_urls_;
  base::flat_map<GURL, ThreatPatternType> dangerous_patterns_;
  base::flat_map<GURL, bool> high_confidence_allowlist_match_urls_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_FAKE_DATABASE_MANAGER_H_
