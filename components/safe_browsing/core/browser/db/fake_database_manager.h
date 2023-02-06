// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_FAKE_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_FAKE_DATABASE_MANAGER_H_

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
  FakeSafeBrowsingDatabaseManager(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  void AddDangerousUrl(const GURL& dangerous_url, SBThreatType threat_type);
  void AddDangerousUrlPattern(const GURL& dangerous_url,
                              ThreatPatternType pattern_type);
  void ClearDangerousUrl(const GURL& dangerous_url);

  // TestSafeBrowsingDatabaseManager implementation:
  // These are implemented as needed to return stubbed values.
  bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      MechanismExperimentHashDatabaseCache experiment_cache_selection) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  safe_browsing::ThreatSource GetThreatSource() const override;

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
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_FAKE_DATABASE_MANAGER_H_
