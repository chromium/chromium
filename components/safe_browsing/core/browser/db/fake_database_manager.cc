// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/fake_database_manager.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/common/features.h"

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

  uintptr_t client_id = reinterpret_cast<uintptr_t>(client);
  pending_clients_.insert(client_id);
  ui_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSafeBrowsingDatabaseManager::CheckBrowseURLAsync,
                     weak_factory_.GetWeakPtr(), url, result_threat_type,
                     client_id));
  return false;
}

void FakeSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  pending_clients_.erase(reinterpret_cast<uintptr_t>(client));
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
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    return safe_browsing::ThreatSource::LOCAL_PVER5_LOCAL_BLOCKLIST;
  }
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

safe_browsing::ThreatSource
FakeSafeBrowsingDatabaseManager::GetNonBrowseUrlThreatSource() const {
  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    return safe_browsing::ThreatSource::LOCAL_PVER5_LOCAL_BLOCKLIST;
  }
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

void FakeSafeBrowsingDatabaseManager::CheckBrowseURLAsync(
    GURL url,
    SBThreatType result_threat_type,
    uintptr_t client_id) {
  if (!pending_clients_.contains(client_id)) {
    return;
  }
  pending_clients_.erase(client_id);
  reinterpret_cast<Client*>(client_id)->OnCheckBrowseUrlResult(
      url, result_threat_type);
}

// static
void FakeSafeBrowsingDatabaseManager::CheckDownloadURLAsync(
    const std::vector<GURL>& url_chain,
    SBThreatType result_threat_type,
    Client* client) {
  client->OnCheckDownloadUrlResult(url_chain, result_threat_type);
}

}  // namespace safe_browsing
