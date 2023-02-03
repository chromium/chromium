// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_TEST_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_TEST_DATABASE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

namespace safe_browsing {

// This is a non-pure-virtual implementation of the SafeBrowsingDatabaseManager
// interface.  It's used in tests by overriding only the functions that get
// called, and it'll complain if you call one that isn't overriden. The
// non-abstract methods in the base class are not overridden.
class TestSafeBrowsingDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  TestSafeBrowsingDatabaseManager(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // SafeBrowsingDatabaseManager implementation:
  void CancelCheck(Client* client) override;
  bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      MechanismExperimentHashDatabaseCache experiment_cache_selection) override;
  AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      const std::string& metric_variation) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  bool MatchDownloadAllowlistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool IsDownloadProtectionEnabled() const override;
  void StartOnIOThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config) override;
  void StopOnIOThread(bool shutdown) override;

 protected:
  ~TestSafeBrowsingDatabaseManager() override = default;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_TEST_DATABASE_MANAGER_H_
