// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager()
    : safe_browsing::TestSafeBrowsingDatabaseManager(
          content::GetUIThreadTaskRunner({})) {}

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
  CHECK(checks_.empty());
  url_to_threat_type_.clear();
}

void FakeSafeBrowsingDatabaseManager::SimulateTimeout() {
  simulate_timeout_ = true;
}

FakeSafeBrowsingDatabaseManager::~FakeSafeBrowsingDatabaseManager() = default;

bool FakeSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (synchronous_failure_ && !url_to_threat_type_.count(url)) {
    return true;
  }

  // Enforce the invariant that a client will not send multiple requests, with
  // the subresource filter client implementation.
  CHECK(checks_.find(client) == checks_.end());
  checks_.insert(client);
  if (simulate_timeout_) {
    return false;
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSafeBrowsingDatabaseManager::
                         OnCheckUrlForSubresourceFilterComplete,
                     weak_factory_.GetWeakPtr(), client->GetWeakPtr(), url));
  return false;
}

void FakeSafeBrowsingDatabaseManager::OnCheckUrlForSubresourceFilterComplete(
    base::WeakPtr<Client> client_weak_ptr,
    const GURL& url) {
  if (!client_weak_ptr) {
    return;
  }
  Client* client = client_weak_ptr.get();
  // Check to see if the request was cancelled to avoid use-after-free.
  if (checks_.find(client) == checks_.end()) {
    return;
  }
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
  // subsequent clients that share an address with this one will CHECK in
  // CheckUrlForSubresourceFilter.
  checks_.erase(client);
}

void FakeSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  size_t erased = checks_.erase(client);
  CHECK_EQ(erased, 1u);
}

safe_browsing::ThreatSource
FakeSafeBrowsingDatabaseManager::GetBrowseUrlThreatSource(
    safe_browsing::CheckBrowseUrlType check_type) const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

safe_browsing::ThreatSource
FakeSafeBrowsingDatabaseManager::GetNonBrowseUrlThreatSource() const {
  return safe_browsing::ThreatSource::LOCAL_PVER4;
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  return true;
}
