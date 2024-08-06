// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_CONTEXT_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_CONTEXT_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/sync/engine/active_devices_invalidation_info.h"
#include "components/sync/engine/cycle/debug_info_getter.h"
#include "components/sync/engine/data_type_registry.h"
#include "components/sync/engine/sync_engine_event_listener.h"

namespace syncer {

class ExtensionsActivity;
class DataTypeRegistry;
class ServerConnectionManager;

// Default number of items a client can commit in a single message.
static const int kDefaultMaxCommitBatchSize = 25;

// SyncCycleContext encapsulates the contextual information and engine
// components specific to a SyncCycle.  Unlike the SyncCycle, the context
// can be reused across several sync cycles.
//
// The context does not take ownership of its pointer members.  It's up to
// the surrounding classes to ensure those members remain valid while the
// context is in use.
//
// It can only be used from the SyncerThread.
class SyncCycleContext {
 public:
  SyncCycleContext(ServerConnectionManager* connection_manager,
                   ExtensionsActivity* extensions_activity,
                   const std::vector<SyncEngineEventListener*>& listeners,
                   DebugInfoGetter* debug_info_getter,
                   DataTypeRegistry* data_type_registry,
                   const std::string& cache_guid,
                   const std::string& birthday,
                   const std::string& bag_of_chips,
                   base::TimeDelta poll_interval);

  SyncCycleContext(const SyncCycleContext&) = delete;
  SyncCycleContext& operator=(const SyncCycleContext&) = delete;

  ~SyncCycleContext();

  ServerConnectionManager* connection_manager() { return connection_manager_; }

  DataTypeSet GetConnectedTypes() const;

  ExtensionsActivity* extensions_activity() {
    return extensions_activity_.get();
  }

  DebugInfoGetter* debug_info_getter() { return debug_info_getter_; }

  // Talk notification status.
  void set_notifications_enabled(bool enabled) {
    notifications_enabled_ = enabled;
  }
  bool notifications_enabled() { return notifications_enabled_; }

  const std::string& cache_guid() const { return cache_guid_; }

  void set_birthday(const std::string& birthday);
  const std::string& birthday() const { return birthday_; }

  void set_bag_of_chips(const std::string& bag_of_chips);
  const std::string& bag_of_chips() const { return bag_of_chips_; }

  void set_account_name(const std::string& name) { account_name_ = name; }
  const std::string& account_name() const { return account_name_; }

  void set_max_commit_batch_size(int batch_size) {
    max_commit_batch_size_ = batch_size;
  }
  int32_t max_commit_batch_size() const { return max_commit_batch_size_; }

  base::ObserverList<SyncEngineEventListener>::Unchecked* listeners() {
    return &listeners_;
  }

  void set_is_sync_feature_enabled(bool value) {
    client_status_.set_is_sync_feature_enabled(value);
  }

  const sync_pb::ClientStatus& client_status() const { return client_status_; }

  DataTypeRegistry* data_type_registry() { return data_type_registry_; }

  bool cookie_jar_mismatch() const { return cookie_jar_mismatch_; }

  void set_cookie_jar_mismatch(bool cookie_jar_mismatch) {
    cookie_jar_mismatch_ = cookie_jar_mismatch;
  }

  base::TimeDelta poll_interval() const { return poll_interval_; }
  void set_poll_interval(base::TimeDelta interval) {
    DCHECK(!interval.is_zero());
    poll_interval_ = interval;
  }

  const ActiveDevicesInvalidationInfo& active_devices_invalidation_info() {
    return active_devices_invalidation_info_;
  }

  void set_active_devices_invalidation_info(
      ActiveDevicesInvalidationInfo active_devices_invalidation_info) {
    active_devices_invalidation_info_ =
        std::move(active_devices_invalidation_info);
  }

 private:
  base::ObserverList<SyncEngineEventListener>::Unchecked listeners_;

  const raw_ptr<ServerConnectionManager, DanglingUntriaged> connection_manager_;

  // We use this to stuff extensions activity into CommitMessages so the server
  // can correlate commit traffic with extension-related bookmark mutations.
  const scoped_refptr<ExtensionsActivity> extensions_activity_;

  // Kept up to date with talk events to determine whether notifications are
  // enabled. True only if the notification channel is authorized and open.
  bool notifications_enabled_ = false;

  const std::string cache_guid_;

  std::string birthday_;

  std::string bag_of_chips_;

  // The name of the account being synced.
  std::string account_name_;

  // The server limits the number of items a client can commit in one batch.
  int max_commit_batch_size_ = kDefaultMaxCommitBatchSize;

  // We use this to get debug info to send to the server for debugging
  // client behavior on server side.
  const raw_ptr<DebugInfoGetter, DanglingUntriaged> debug_info_getter_;

  const raw_ptr<DataTypeRegistry> data_type_registry_;

  // Satus information to be sent up to the server.
  sync_pb::ClientStatus client_status_;

  // Whether the account(s) present in the content area's cookie jar match the
  // chrome account. If multiple accounts are present in the cookie jar, a
  // mismatch implies all of them are different from the chrome account.
  bool cookie_jar_mismatch_ = false;

  ActiveDevicesInvalidationInfo active_devices_invalidation_info_ =
      ActiveDevicesInvalidationInfo::CreateUninitialized();

  base::TimeDelta poll_interval_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_SYNC_CYCLE_CONTEXT_H_
