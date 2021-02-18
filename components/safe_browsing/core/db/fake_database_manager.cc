// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/db/fake_database_manager.h"

#include "components/safe_browsing/core/common/thread_utils.h"

namespace safe_browsing {

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager() = default;
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

bool FakeSafeBrowsingDatabaseManager::CanCheckRequestDestination(
    network::mojom::RequestDestination request_destination) const {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool FakeSafeBrowsingDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client) {
  const auto it = dangerous_urls_.find(url);
  if (it == dangerous_urls_.end())
    return true;

  const SBThreatType result_threat_type = it->second;
  if (result_threat_type == SB_THREAT_TYPE_SAFE)
    return true;

  GetTaskRunner(ThreadID::IO)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&FakeSafeBrowsingDatabaseManager::CheckBrowseURLAsync,
                         url, result_threat_type, client));
  return false;
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  return true;
}

safe_browsing::ThreatSource FakeSafeBrowsingDatabaseManager::GetThreatSource()
    const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

bool FakeSafeBrowsingDatabaseManager::IsSupported() const {
  return true;
}

// static
void FakeSafeBrowsingDatabaseManager::CheckBrowseURLAsync(
    GURL url,
    SBThreatType result_threat_type,
    Client* client) {
  client->OnCheckBrowseUrlResult(url, result_threat_type,
                                 safe_browsing::ThreatMetadata());
}

}  // namespace safe_browsing
