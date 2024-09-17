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
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : SafeBrowsingDatabaseManager(std::move(ui_task_runner)), enabled_(false) {}

void TestSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  NOTIMPLEMENTED();
}

bool TestSafeBrowsingDatabaseManager::CanCheckUrl(const GURL& url) const {
  NOTIMPLEMENTED();
  return (url != GURL("about:blank"));
}

bool TestSafeBrowsingDatabaseManager::CheckBrowseUrl(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    Client* client,
    CheckBrowseUrlType check_type) {
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

void TestSafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    CheckUrlForHighConfidenceAllowlistCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(
      /*url_on_high_confidence_allowlist=*/false,
      /*logging_details=*/std::nullopt);
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

void TestSafeBrowsingDatabaseManager::MatchDownloadAllowlistUrl(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(true);
}

safe_browsing::ThreatSource
TestSafeBrowsingDatabaseManager::GetBrowseUrlThreatSource(
    CheckBrowseUrlType check_type) const {
  NOTIMPLEMENTED();
  return safe_browsing::ThreatSource::UNKNOWN;
}

safe_browsing::ThreatSource
TestSafeBrowsingDatabaseManager::GetNonBrowseUrlThreatSource() const {
  NOTIMPLEMENTED();
  return safe_browsing::ThreatSource::UNKNOWN;
}

void TestSafeBrowsingDatabaseManager::StartOnUIThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnUIThread(url_loader_factory, config);
  enabled_ = true;
}

void TestSafeBrowsingDatabaseManager::StopOnUIThread(bool shutdown) {
  enabled_ = false;
  SafeBrowsingDatabaseManager::StopOnUIThread(shutdown);
}

bool TestSafeBrowsingDatabaseManager::IsDatabaseReady() const {
  return enabled_;
}

}  // namespace safe_browsing
