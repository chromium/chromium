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

namespace safe_browsing {

// This is a non-pure-virtual implementation of the SafeBrowsingDatabaseManager
// interface.  It's used in tests by overriding only the functions that get
// called, and it'll complain if you call one that isn't overriden. The
// non-abstract methods in the base class are not overridden.
class TestSafeBrowsingDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  explicit TestSafeBrowsingDatabaseManager(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // SafeBrowsingDatabaseManager implementation:
  void CancelCheck(Client* client) override;
  bool CanCheckUrl(const GURL& url) const override;
  bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      CheckBrowseUrlType check_type) override;
  AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      CheckUrlForHighConfidenceAllowlistCallback callback) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  void MatchDownloadAllowlistUrl(
      const GURL& url,
      base::OnceCallback<void(bool)> callback) override;
  safe_browsing::ThreatSource GetBrowseUrlThreatSource(
      CheckBrowseUrlType check_type) const override;
  safe_browsing::ThreatSource GetNonBrowseUrlThreatSource() const override;
  void StartOnUIThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config) override;
  void StopOnUIThread(bool shutdown) override;
  bool IsDatabaseReady() const override;

 protected:
  ~TestSafeBrowsingDatabaseManager() override = default;

 private:
  bool enabled_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_TEST_DATABASE_MANAGER_H_
