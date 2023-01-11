// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager()
    : safe_browsing::TestSafeBrowsingDatabaseManager(
          content::GetUIThreadTaskRunner({}),
          content::GetIOThreadTaskRunner({})) {}

void FakeSafeBrowsingDatabaseManager::AddBlocklistedUrl(
    const GURL& url,
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& metadata) {
  url_to_threat_type_[url] = std::make_pair(threat_type, metadata);
}

void FakeSafeBrowsingDatabaseManager::AddBlocklistedUrl(
    const GURL& url,
    safe_browsing::SBThreatType threat_type,
    safe_browsing::ThreatPatternType pattern_type) {
  safe_browsing::ThreatMetadata metadata;
  metadata.threat_pattern_type = pattern_type;
  AddBlocklistedUrl(url, threat_type, metadata);
}

void FakeSafeBrowsingDatabaseManager::RemoveBlocklistedUrl(const GURL& url) {
  url_to_threat_type_.erase(url);
}

void FakeSafeBrowsingDatabaseManager::RemoveAllBlocklistedUrls() {
  DCHECK(checks_.empty());
  url_to_threat_type_.clear();
}

void FakeSafeBrowsingDatabaseManager::SimulateTimeout() {
  simulate_timeout_ = true;
}

FakeSafeBrowsingDatabaseManager::~FakeSafeBrowsingDatabaseManager() {}

bool FakeSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  if (synchronous_failure_ && !url_to_threat_type_.count(url))
    return true;

  // Enforce the invariant that a client will not send multiple requests, with
  // the subresource filter client implementation.
  DCHECK(checks_.find(client) == checks_.end());
  checks_.insert(client);
  if (simulate_timeout_)
    return false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FakeSafeBrowsingDatabaseManager::
                                    OnCheckUrlForSubresourceFilterComplete,
                                weak_factory_.GetWeakPtr(),
                                base::Unretained(client), url));
  return false;
}

void FakeSafeBrowsingDatabaseManager::OnCheckUrlForSubresourceFilterComplete(
    Client* client,
    const GURL& url) {
  // Check to see if the request was cancelled to avoid use-after-free.
  if (checks_.find(client) == checks_.end())
    return;
  safe_browsing::ThreatMetadata metadata;
  safe_browsing::SBThreatType threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE;
  auto it = url_to_threat_type_.find(url);
  if (it != url_to_threat_type_.end()) {
    threat_type = it->second.first;
    metadata = it->second.second;
  }
  client->OnCheckBrowseUrlResult(url, threat_type, metadata);

  // Erase the client when a check is complete. Otherwise, it's possible
  // subsequent clients that share an address with this one will DCHECK in
  // CheckUrlForSubresourceFilter.
  checks_.erase(client);
}

bool FakeSafeBrowsingDatabaseManager::CheckResourceUrl(const GURL& url,
                                                       Client* client) {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}
void FakeSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  size_t erased = checks_.erase(client);
  DCHECK_EQ(erased, 1u);
}
bool FakeSafeBrowsingDatabaseManager::CanCheckRequestDestination(
    network::mojom::RequestDestination /* request_destination */) const {
  return true;
}

safe_browsing::ThreatSource FakeSafeBrowsingDatabaseManager::GetThreatSource()
    const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  return true;
}
