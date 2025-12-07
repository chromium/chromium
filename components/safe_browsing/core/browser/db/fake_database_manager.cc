// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/util.h"

namespace safe_browsing {

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : TestSafeBrowsingDatabaseManager(std::move(ui_task_runner)) {}

FakeSafeBrowsingDatabaseManager::~FakeSafeBrowsingDatabaseManager() = default;

void FakeSafeBrowsingDatabaseManager::AddDangerousUrl(
    const GURL& dangerous_url,
    SBThreatType threat_type) {
  dangerous_urls_[dangerous_url] = threat_type;
}

void FakeSafeBrowsingDatabaseManager::AddDangerousUrlPattern(
    const GURL& dangerous_url,
    ThreatPatternType pattern_type) {
  dangerous_patterns_[dangerous_url] = pattern_type;
}

void FakeSafeBrowsingDatabaseManager::ClearDangerousUrl(
    const GURL& dangerous_url) {
  dangerous_urls_.erase(dangerous_url);
}

void FakeSafeBrowsingDatabaseManager::SetHighConfidenceAllowlistMatchResult(
    const GURL& url,
    bool match_allowlist) {
  high_confidence_allowlist_match_urls_[url] = match_allowlist;
}

bool FakeSafeBrowsingDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client,
    CheckBrowseUrlType check_type) {
  const auto it = dangerous_urls_.find(url);
  if (it == dangerous_urls_.end())
    return true;

  const SBThreatType result_threat_type = it->second;
  if (result_threat_type == SBThreatType::SB_THREAT_TYPE_SAFE) {
    return true;
  }

  ThreatPatternType pattern_type = ThreatPatternType::NONE;
  const auto it1 = dangerous_patterns_.find(url);
  if (it1 != dangerous_patterns_.end()) {
    pattern_type = it1->second;
  }

  ui_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSafeBrowsingDatabaseManager::CheckBrowseURLAsync, url,
                     result_threat_type, pattern_type, client));
  return false;
}

bool FakeSafeBrowsingDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  for (size_t i = 0; i < url_chain.size(); i++) {
    GURL url = url_chain[i];

    const auto it = dangerous_urls_.find(url);
    if (it == dangerous_urls_.end())
      continue;

    const SBThreatType result_threat_type = it->second;
    if (result_threat_type == SBThreatType::SB_THREAT_TYPE_SAFE) {
      continue;
    }

    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSafeBrowsingDatabaseManager::CheckDownloadURLAsync,
                       url_chain, result_threat_type, client));
    return false;
  }

  return true;
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  return true;
}

void FakeSafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    CheckUrlForHighConfidenceAllowlistCallback callback) {
  const auto it = high_confidence_allowlist_match_urls_.find(url);
  bool on_high_confidence_allowlist =
      (it != high_confidence_allowlist_match_urls_.end() && it->second);
  std::move(callback).Run(on_high_confidence_allowlist,
                          /*logging_details=*/std::nullopt);
}

bool FakeSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  return true;
}

safe_browsing::ThreatSource
FakeSafeBrowsingDatabaseManager::GetBrowseUrlThreatSource(
    CheckBrowseUrlType check_type) const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

safe_browsing::ThreatSource
FakeSafeBrowsingDatabaseManager::GetNonBrowseUrlThreatSource() const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

// static
void FakeSafeBrowsingDatabaseManager::CheckBrowseURLAsync(
    GURL url,
    SBThreatType result_threat_type,
    ThreatPatternType pattern_type,
    Client* client) {
  ThreatMetadata metadata;
  metadata.threat_pattern_type = pattern_type;
  client->OnCheckBrowseUrlResult(url, result_threat_type, metadata);
}

// static
void FakeSafeBrowsingDatabaseManager::CheckDownloadURLAsync(
    const std::vector<GURL>& url_chain,
    SBThreatType result_threat_type,
    Client* client) {
  client->OnCheckDownloadUrlResult(url_chain, result_threat_type);
}

}  // namespace safe_browsing
