// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "content/public/common/resource_type.h"

class GURL;

// Database manager that allows any URL to be configured as blacklisted for
// testing.
class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager();

  void AddBlacklistedUrl(const GURL& url,
                         safe_browsing::SBThreatType threat_type,
                         const safe_browsing::ThreatMetadata& metadata);
  void AddBlacklistedUrl(const GURL& url,
                         safe_browsing::SBThreatType threat_type,
                         safe_browsing::ThreatPatternType pattern_type =
                             safe_browsing::ThreatPatternType::NONE);
  void RemoveBlacklistedUrl(const GURL& url);
  void RemoveAllBlacklistedUrls();

  void SimulateTimeout();

  // If set, will synchronously fail from CheckUrlForSubresourceFilter rather
  // than posting a task to fail if the URL does not match the blacklist.
  void set_synchronous_failure() { synchronous_failure_ = true; }

 protected:
  ~FakeSafeBrowsingDatabaseManager() override;

  // safe_browsing::TestSafeBrowsingDatabaseManager:
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool IsSupported() const override;
  void CancelCheck(Client* client) override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(
      content::ResourceType /* resource_type */) const override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;

 private:
  void OnCheckUrlForSubresourceFilterComplete(Client* client, const GURL& url);

  std::set<Client*> checks_;
  std::map<
      GURL,
      std::pair<safe_browsing::SBThreatType, safe_browsing::ThreatMetadata>>
      url_to_threat_type_;
  bool simulate_timeout_ = false;
  bool synchronous_failure_ = false;

  base::WeakPtrFactory<FakeSafeBrowsingDatabaseManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
