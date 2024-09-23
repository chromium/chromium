// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/database_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
namespace safe_browsing {
SafeBrowsingDatabaseManager::Client::Client() = default;
SafeBrowsingDatabaseManager::Client::~Client() = default;

base::WeakPtr<SafeBrowsingDatabaseManager::Client>
SafeBrowsingDatabaseManager::Client::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

SafeBrowsingDatabaseManager::SafeBrowsingDatabaseManager(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : base::RefCountedDeleteOnSequence<SafeBrowsingDatabaseManager>(
          std::move(ui_task_runner)) {}

SafeBrowsingDatabaseManager::~SafeBrowsingDatabaseManager() {
  DCHECK(!v4_get_hash_protocol_manager_);
}

bool SafeBrowsingDatabaseManager::CancelApiCheck(Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  auto it = FindClientApiCheck(client);
  if (it != api_checks_.end()) {
    api_checks_.erase(it);
    return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool SafeBrowsingDatabaseManager::CheckApiBlocklistUrl(const GURL& url,
                                                       Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Make sure we can check this url and that the service is enabled.
  if (!IsDatabaseReady() ||
      !(url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    return true;
  }

  // There can only be one in-progress check for the same client at a time.
  DCHECK(FindClientApiCheck(client) == api_checks_.end());

  std::unique_ptr<SafeBrowsingApiCheck> check(
      new SafeBrowsingApiCheck(url, client));
  api_checks_.insert(check.get());

  std::vector<std::string> list_client_states;
  V4ProtocolManagerUtil::GetListClientStatesFromStoreStateMap(
      GetStoreStateMap(), &list_client_states);

  v4_get_hash_protocol_manager_->GetFullHashesWithApis(
      url, list_client_states,
      base::BindOnce(&SafeBrowsingDatabaseManager::OnThreatMetadataResponse,
                     base::Unretained(this), std::move(check)));

  return false;
}

SafeBrowsingDatabaseManager::ApiCheckSet::iterator
SafeBrowsingDatabaseManager::FindClientApiCheck(Client* client) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  for (auto it = api_checks_.begin(); it != api_checks_.end(); ++it) {
    if ((*it)->client() == client) {
      return it;
    }
  }
  return api_checks_.end();
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
    std::unique_ptr<SafeBrowsingApiCheck> check,
    const ThreatMetadata& md) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(check);

  // If the check is not in |api_checks_| then the request was cancelled by the
  // client.
  auto it = api_checks_.find(check.get());
  if (it == api_checks_.end())
    return;

  check->client()->OnCheckApiBlocklistUrlResult(check->url(), md);
  api_checks_.erase(it);
}

void SafeBrowsingDatabaseManager::StartOnUIThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  v4_get_hash_protocol_manager_ = V4GetHashProtocolManager::Create(
      url_loader_factory, GetStoresForFullHashRequests(), config);
}

// |shutdown| not used. Destroys the v4 protocol managers. This may be called
// multiple times during the life of the DatabaseManager.
void SafeBrowsingDatabaseManager::StopOnUIThread(bool shutdown) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Delete pending checks, calling back any clients with empty metadata.
  for (const SafeBrowsingApiCheck* check : api_checks_) {
    if (check->client()) {
      check->client()->OnCheckApiBlocklistUrlResult(check->url(),
                                                    ThreatMetadata());
    }
  }

  // This cancels all in-flight GetHash requests.
  v4_get_hash_protocol_manager_.reset();
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

SafeBrowsingDatabaseManager::SafeBrowsingApiCheck::SafeBrowsingApiCheck(
    const GURL& url,
    Client* client)
    : url_(url), client_(client) {}

}  // namespace safe_browsing
