// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingDatabaseManager that sends URLs
// via IPC to a database that chromium doesn't manage locally.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_REMOTE_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_REMOTE_DATABASE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace safe_browsing {

struct V4ProtocolConfig;

// An implementation that proxies requests to a service outside of Chromium.
// Does not manage a local database.
class RemoteSafeBrowsingDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  // Construct RemoteSafeBrowsingDatabaseManager.
  // Must be initialized by calling StartOnSBThread() before using.
  RemoteSafeBrowsingDatabaseManager();

  RemoteSafeBrowsingDatabaseManager(const RemoteSafeBrowsingDatabaseManager&) =
      delete;
  RemoteSafeBrowsingDatabaseManager& operator=(
      const RemoteSafeBrowsingDatabaseManager&) = delete;

  //
  // SafeBrowsingDatabaseManager implementation
  //

  void CancelCheck(Client* client) override;
  bool CanCheckRequestDestination(
      network::mojom::RequestDestination request_destination) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CheckBrowseUrl(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      Client* client,
      MechanismExperimentHashDatabaseCache experiment_cache_selection,
      CheckBrowseUrlType check_type) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  void CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      const std::string& metric_variation,
      base::OnceCallback<void(bool)> callback) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  void MatchDownloadAllowlistUrl(
      const GURL& url,
      base::OnceCallback<void(bool)> callback) override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool IsDownloadProtectionEnabled() const override;
  void StartOnSBThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config) override;
  void StopOnSBThread(bool shutdown) override;
  bool IsDatabaseReady() const override;

  //
  // RemoteSafeBrowsingDatabaseManager implementation
  //

 private:
  class ClientRequest;  // Per-request tracker.
  friend class base::RefCountedThreadSafe<RemoteSafeBrowsingDatabaseManager>;

  ~RemoteSafeBrowsingDatabaseManager() override;

  // Requests currently outstanding.  This owns the ptrs.
  std::vector<ClientRequest*> current_requests_;

  base::flat_set<network::mojom::RequestDestination>
      request_destinations_to_check_;

  // Whether the service is running. 'enabled_' is used by the
  // RemoteSafeBrowsingDatabaseManager on the IO thread during normal
  // operations.
  bool enabled_;
};  // class RemoteSafeBrowsingDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_REMOTE_DATABASE_MANAGER_H_
