// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/db/test_database_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

void TestSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  NOTIMPLEMENTED();
}

bool TestSafeBrowsingDatabaseManager::CanCheckResourceType(
    content::ResourceType resource_type) const {
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
    Client* client) {
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

AsyncMatch TestSafeBrowsingDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return AsyncMatch::NO_MATCH;
}

bool TestSafeBrowsingDatabaseManager::CheckUrlForSubresourceFilter(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return true;
}

AsyncMatch TestSafeBrowsingDatabaseManager::CheckCsdWhitelistUrl(
    const GURL& url,
    Client* client) {
  NOTIMPLEMENTED();
  return AsyncMatch::MATCH;
}

bool TestSafeBrowsingDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  NOTIMPLEMENTED();
  return true;
}

bool TestSafeBrowsingDatabaseManager::MatchDownloadWhitelistUrl(
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

bool TestSafeBrowsingDatabaseManager::IsSupported() const {
  NOTIMPLEMENTED();
  return false;
}

void TestSafeBrowsingDatabaseManager::StartOnIOThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnIOThread(url_loader_factory, config);
  enabled_ = true;
}

void TestSafeBrowsingDatabaseManager::StopOnIOThread(bool shutdown) {
  enabled_ = false;
  SafeBrowsingDatabaseManager::StopOnIOThread(shutdown);
}

}  // namespace safe_browsing
