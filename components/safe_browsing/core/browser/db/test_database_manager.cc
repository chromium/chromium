// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/test_database_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

TestSafeBrowsingDatabaseManager::TestSafeBrowsingDatabaseManager(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : SafeBrowsingDatabaseManager(std::move(ui_task_runner),
                                  std::move(io_task_runner)) {}

void TestSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  NOTIMPLEMENTED();
}

bool TestSafeBrowsingDatabaseManager::CanCheckRequestDestination(
    network::mojom::RequestDestination request_destination) const {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::CanCheckUrl(const GURL& url) const {
  NOTIMPLEMENTED();
  return (url != GURL("about:blank"));
}

bool TestSafeBrowsingDatabaseManager::ChecksAreAlwaysAsync() const {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client,
    MechanismExperimentHashDatabaseCache experiment_cache_selection) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckResourceUrl(const GURL& url,
                                                       Client* client) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    const std::string& metric_variation) {
  NOTIMPLEMENTED();
  return false;
}

bool TestSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

AsyncMatch TestSafeBrowsingDatabaseManager::CheckCsdAllowlistUrl(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return AsyncMatch::MATCH;
}

bool TestSafeBrowsingDatabaseManager::MatchDownloadAllowlistUrl(
    const GURL& url) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::MatchMalwareIP(
    const std::string& ip_address) {
  NOTIMPLEMENTED();
  return true;
}

safe_browsing::ThreatSource TestSafeBrowsingDatabaseManager::GetThreatSource()
    const {
  NOTIMPLEMENTED();
  return safe_browsing::ThreatSource::UNKNOWN;
}

bool TestSafeBrowsingDatabaseManager::IsDownloadProtectionEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

void TestSafeBrowsingDatabaseManager::StartOnSBThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnSBThread(url_loader_factory, config);
  enabled_ = true;
}

void TestSafeBrowsingDatabaseManager::StopOnSBThread(bool shutdown) {
  enabled_ = false;
  SafeBrowsingDatabaseManager::StopOnSBThread(shutdown);
}

}  // namespace safe_browsing
