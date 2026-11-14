// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/network_name_cache.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "net/base/network_interfaces.h"

namespace policy {

// static
std::unique_ptr<NetworkNameCache> NetworkNameCache::CreateForTesting(
    Provider provider) {
  return base::WrapUnique(new NetworkNameCache(std::move(provider)));
}

NetworkNameCache::NetworkNameCache(base::PassKey<BrowserPolicyConnector>)
    : NetworkNameCache(base::BindRepeating(&net::GetWifiSSID)) {}

NetworkNameCache::NetworkNameCache(Provider provider)
    : provider_(std::move(provider)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  CHECK(provider_);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

NetworkNameCache::~NetworkNameCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

std::string NetworkNameCache::GetNetworkName() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!requested_) {
    requested_ = true;
    Refresh();
  }
  return network_name_;
}

void NetworkNameCache::SetProviderForTesting(Provider provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  provider_ =
      provider ? std::move(provider) : base::BindRepeating(&net::GetWifiSSID);
  network_name_.clear();
  if (requested_) {
    Refresh();
  } else {
    tracker_.TryCancelAll();
  }
}

void NetworkNameCache::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `NetworkChangeObserver` is always notified with `CONNECTION_NONE` before
  // the new connection type, so the cached name is cleared right away and
  // repopulated once the query for the new network completes.
  network_name_.clear();
  if (!requested_ || type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    tracker_.TryCancelAll();
    return;
  }
  Refresh();
}

void NetworkNameCache::Refresh() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Supersede earlier queries: queued ones never run the provider, and the
  // reply of one that is already running (possibly hung) is dropped. Since
  // `task_runner_` is sequenced, the new query waits behind a running one
  // instead of blocking a second worker thread.
  tracker_.TryCancelAll();
  tracker_.PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::OnceCallback<std::string()>(provider_),
      // Unretained is safe: `tracker_` is owned by `this` and drops pending
      // replies when destroyed.
      base::BindOnce(&NetworkNameCache::OnRefreshed, base::Unretained(this)));
}

void NetworkNameCache::OnRefreshed(std::string network_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_name_ = std::move(network_name);
}

}  // namespace policy
