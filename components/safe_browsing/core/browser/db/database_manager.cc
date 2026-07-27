// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/database_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"
namespace safe_browsing {
SafeBrowsingDatabaseManager::Client::Client(base::PassKey<Client> pass_key) {}
SafeBrowsingDatabaseManager::Client::~Client() = default;

base::WeakPtr<SafeBrowsingDatabaseManager::Client>
SafeBrowsingDatabaseManager::Client::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// TODO(crbug.com/362791941): Remove default once all clients override this.
base::WeakPtr<V5GetHashProtocolManager>
SafeBrowsingDatabaseManager::Client::GetV5GetHashProtocolManager() {
  return nullptr;
}

base::PassKey<SafeBrowsingDatabaseManager::Client>
SafeBrowsingDatabaseManager::Client::GetPassKeyForTesting() {
  return base::PassKey<Client>();
}

base::PassKey<SafeBrowsingDatabaseManager::Client>
SafeBrowsingDatabaseManager::Client::GetPassKey() {
  return base::PassKey<Client>();
}

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : base::RefCountedDeleteOnSequence<SafeBrowsingDatabaseManager>(
          std::move(ui_task_runner)) {}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
  DCHECK(!v4_get_hash_protocol_manager_);
}

bool SafeBrowsingDatabaseManager::CancelNotificationAbuseCheck(Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  CHECK(client);
  auto it = FindClientNotificationAbuseCheck(client);
  if (it != notification_abuse_checks_.end()) {
    notification_abuse_checks_.erase(it);
    return true;
  }
  NOTREACHED();
}

bool SafeBrowsingDatabaseManager::CheckNotificationAbuseUrl(const GURL& url,
                                                            Client* client) {
  CHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  CHECK(client);

  // Make sure we can check this url and that the service is enabled.
  if (!IsDatabaseReady() ||
      !(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    return true;
  }

  if (base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    base::WeakPtr<V5GetHashProtocolManager> v5_get_hash_protocol_manager =
        client->GetV5GetHashProtocolManager();
    if (!v5_get_hash_protocol_manager) {
      return true;
    }

    // There can only be one in-progress check for the same client at a time.
    CHECK(FindClientNotificationAbuseCheck(client) ==
          notification_abuse_checks_.end());

    std::unique_ptr<NotificationAbuseCheck> check =
        std::make_unique<NotificationAbuseCheck>(url, client);
    notification_abuse_checks_.insert(check.get());

    std::vector<FullHashStr> full_hashes;
    SBProtocolManagerUtil::UrlToFullHashes(url::Origin::Create(url).GetURL(),
                                           &full_hashes);
    std::map<FullHashStr, std::vector<SBThreatType>> full_hash_to_threat_types;
    for (FullHashStr& full_hash : full_hashes) {
      full_hash_to_threat_types.emplace(
          std::move(full_hash),
          std::vector<SBThreatType>{SBThreatType::SB_THREAT_TYPE_API_ABUSE});
    }
    v5_get_hash_protocol_manager->GetFullHashes(
        full_hash_to_threat_types,
        // Wrap with WrapCallbackWithDefaultInvokeIfNotRun to ensure
        // OnNotificationAbuseFullHashesResponse runs if
        // V5GetHashProtocolManager is destroyed, which ensure the caller gets
        // a response and removes the check from notification_abuse_checks_.
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&SafeBrowsingDatabaseManager::
                               OnNotificationAbuseFullHashesResponse,
                           weak_factory_.GetWeakPtr(), std::move(check)),
            /*threat_type=*/SBThreatType::SB_THREAT_TYPE_SAFE,
            /*metadata=*/ThreatMetadata()));
    return false;
  }

  CHECK(v4_get_hash_protocol_manager_);

  // There can only be one in-progress check for the same client at a time.
  DCHECK(FindClientNotificationAbuseCheck(client) ==
         notification_abuse_checks_.end());

  std::unique_ptr<NotificationAbuseCheck> check(
      new NotificationAbuseCheck(url, client));
  notification_abuse_checks_.insert(check.get());

  std::vector<std::string> list_client_states;
  SBProtocolManagerUtil::GetListClientStatesFromStoreStateMap(
      GetStoreStateMap(), &list_client_states);

  v4_get_hash_protocol_manager_->GetFullHashesForNotificationAbuse(
      url, list_client_states,
      base::BindOnce(&SafeBrowsingDatabaseManager::OnThreatMetadataResponse,
                     base::Unretained(this), std::move(check)));

  return false;
}

SafeBrowsingDatabaseManager::NotificationAbuseCheckSet::iterator
SafeBrowsingDatabaseManager::FindClientNotificationAbuseCheck(Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  for (auto it = notification_abuse_checks_.begin();
       it != notification_abuse_checks_.end(); ++it) {
    if ((*it)->client() == client) {
      return it;
    }
  }
  return notification_abuse_checks_.end();
}

// Keep the list returned here in sync with GetStoreStateMap()
StoresToCheck SafeBrowsingDatabaseManager::GetStoresForFullHashRequests() {
  return StoresToCheck({GetChromeUrlApiId()});
}

// Keep the list returned here in sync with GetStoresForFullHashRequests()
std::unique_ptr<StoreStateMap> SafeBrowsingDatabaseManager::GetStoreStateMap() {
  // This implementation is currently used only for RemoteDatabaseManager which
  // only requests full hashes for GetChromeUrlApiId() list that has no local
  // storage so the client state is always empty.

  auto store_state_map = std::make_unique<StoreStateMap>();
  (*store_state_map)[GetChromeUrlApiId()] = "";
  return store_state_map;
}

void SafeBrowsingDatabaseManager::OnThreatMetadataResponse(
    std::unique_ptr<NotificationAbuseCheck> check,
    bool is_abusive) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(check);

  // If the check is not in |notification_abuse_checks_| then the request was
  // cancelled by the client.
  auto it = notification_abuse_checks_.find(check.get());
  if (it == notification_abuse_checks_.end()) {
    return;
  }

  check->client()->OnCheckNotificationAbuseUrlResult(is_abusive);
  notification_abuse_checks_.erase(it);
}

void SafeBrowsingDatabaseManager::OnNotificationAbuseFullHashesResponse(
    std::unique_ptr<NotificationAbuseCheck> check,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  CHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  CHECK(check);

  OnThreatMetadataResponse(
      std::move(check),
      /*is_abusive=*/threat_type == SBThreatType::SB_THREAT_TYPE_API_ABUSE);
}

void SafeBrowsingDatabaseManager::StartOnUIThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  if (!base::FeatureList::IsEnabled(kLocalListsUseSBv5)) {
    v4_get_hash_protocol_manager_ = V4GetHashProtocolManager::Create(
        url_loader_factory, GetStoresForFullHashRequests(), config);
  }
}

// |shutdown| not used. Destroys the v4 protocol managers. This may be called
// multiple times during the life of the DatabaseManager.
void SafeBrowsingDatabaseManager::StopOnUIThread(bool shutdown) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Delete pending checks, calling back any clients with empty metadata.
  NotificationAbuseCheckSet checks =
      std::exchange(notification_abuse_checks_, {});
  for (const NotificationAbuseCheck* check : checks) {
    CHECK(check->client());
    check->client()->OnCheckNotificationAbuseUrlResult(/*is_abusive=*/false);
  }

  // This cancels all in-flight GetHash requests.
  v4_get_hash_protocol_manager_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

base::CallbackListSubscription
SafeBrowsingDatabaseManager::RegisterDatabaseUpdatedCallback(
    const OnDatabaseUpdated& cb) {
  return update_complete_callback_list_.Add(cb);
}

void SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  update_complete_callback_list_.Notify();
}

SafeBrowsingDatabaseManager::NotificationAbuseCheck::NotificationAbuseCheck(
    const GURL& url,
    Client* client)
    : url_(url), client_(client) {
  CHECK(client_);
}

}  // namespace safe_browsing
