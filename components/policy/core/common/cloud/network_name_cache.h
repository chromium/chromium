// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_NETWORK_NAME_CACHE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_NETWORK_NAME_CACHE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "components/policy/policy_export.h"
#include "net/base/network_change_notifier.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class BrowserPolicyConnector;

// Caches the name (SSID) of the Wi-Fi network the device is connected to.
//
// `net::GetWifiSSID()` can block for a long time (e.g. the WLAN API on
// Windows 11 24H2+), so it is only ever invoked on a dedicated `MayBlock`
// sequence, and consumers read the last known value synchronously. The network
// name is not queried until it is first requested via `GetNetworkName()`; from
// then on, it is refreshed whenever `net::NetworkChangeNotifier` reports a
// network change.
//
// Only `BrowserPolicyConnector` can create an instance, which it hands over to
// the `DeviceManagementService`; reporting code reaches it through that
// service. This ensures that a single instance (and thus at most one pending
// `net::GetWifiSSID()` query) exists in production. Tests use
// `CreateForTesting()`. It must be created, used and destroyed on the same
// sequence (the UI thread in the browser process).
class POLICY_EXPORT NetworkNameCache
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  using Provider = base::RepeatingCallback<std::string()>;

  // Creates an instance that uses `provider` to query the network name.
  // `provider` is run on a background sequence.
  static std::unique_ptr<NetworkNameCache> CreateForTesting(Provider provider);

  // Uses `net::GetWifiSSID()` to query the network name.
  explicit NetworkNameCache(base::PassKey<BrowserPolicyConnector>);
  NetworkNameCache(const NetworkNameCache&) = delete;
  NetworkNameCache& operator=(const NetworkNameCache&) = delete;
  ~NetworkNameCache() override;

  // Returns the last known network name. Returns an empty string if the name
  // is not known yet, is being refreshed, or the device is offline. The first
  // call starts the initial background query.
  std::string GetNetworkName();

  // Replaces the provider, clears the cached name and re-queries it if it has
  // been requested before. A null `provider` restores `net::GetWifiSSID()`.
  void SetProviderForTesting(Provider provider);

 private:
  // Uses `provider` to query the network name. `provider` is run on a
  // background sequence.
  explicit NetworkNameCache(Provider provider);

  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  void Refresh();
  void OnRefreshed(std::string network_name);

  SEQUENCE_CHECKER(sequence_checker_);

  Provider provider_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string network_name_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Whether `GetNetworkName()` has been called. Until then, no query is made,
  // so that the network name is only fetched when something needs it.
  bool requested_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Runs queries in order, so that a hung query blocks at most one worker.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Cancels queued queries and drops replies of superseded ones. Must be
  // declared last so that it is destroyed first.
  base::CancelableTaskTracker tracker_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_NETWORK_NAME_CACHE_H_
