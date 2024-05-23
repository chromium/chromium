// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_MOCK_SAFE_BROWSING_DATABASE_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_MOCK_SAFE_BROWSING_DATABASE_MANAGER_H_

#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager();

  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  bool CheckBrowseUrl(const GURL& gurl,
                      const safe_browsing::SBThreatTypeSet& threat_types,
                      Client* client,
                      safe_browsing::CheckBrowseUrlType check_type) override;

  void CancelCheck(Client* client) override;

  bool HasCalledCancelCheck() { return called_cancel_check_; }

  void SetThreatTypeForUrl(GURL gurl, safe_browsing::SBThreatType threat_type) {
    urls_threat_type_[gurl.spec()] = threat_type;
  }

 protected:
  ~MockSafeBrowsingDatabaseManager() override;

 private:
  void OnCheckBrowseURLDone(const GURL& gurl, base::WeakPtr<Client> client);

  base::flat_map<std::string, safe_browsing::SBThreatType> urls_threat_type_;
  bool called_cancel_check_ = false;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_MOCK_SAFE_BROWSING_DATABASE_MANAGER_H_
