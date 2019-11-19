// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple system resources class that uses the current thread for scheduling.
// Assumes the current thread is already running tasks.

#ifndef COMPONENTS_INVALIDATION_IMPL_SYNC_SYSTEM_RESOURCES_H_
#define COMPONENTS_INVALIDATION_IMPL_SYNC_SYSTEM_RESOURCES_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "components/invalidation/impl/state_writer.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidator_state.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "google_apis/gaia/core_account_id.h"
#include "jingle/notifier/base/notifier_options.h"

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace syncer {

class GCMNetworkChannelDelegate;

class SyncLogger : public invalidation::Logger {
 public:
  SyncLogger();

  ~SyncLogger() override;

  // invalidation::Logger implementation.
  void Log(LogLevel level,
           const char* file,
           int line,
           const char* format,
           ...) override;

  void SetSystemResources(invalidation::SystemResources* resources) override;
};

class SyncInvalidationScheduler : public invalidation::Scheduler {
 public:
  SyncInvalidationScheduler();

  ~SyncInvalidationScheduler() override;

  // Start and stop the scheduler.
  void Start();
  void Stop();

  // invalidation::Scheduler implementation.
  void Schedule(invalidation::TimeDelta delay,
                invalidation::Closure* task) override;

  bool IsRunningOnThread() const override;

  invalidation::Time GetCurrentTime() const override;

  void SetSystemResources(invalidation::SystemResources* resources) override;

 private:
  // Runs the task, deletes it, and removes it from |posted_tasks_|.
  void RunPostedTask(invalidation::Closure* task);

  // Holds all posted tasks that have not yet been run.
  std::set<std::unique_ptr<invalidation::Closure>, base::UniquePtrComparator>
      posted_tasks_;

  scoped_refptr<base::SingleThreadTaskRunner> const created_on_task_runner_;
  bool is_started_;
  bool is_stopped_;

  base::WeakPtrFactory<SyncInvalidationScheduler> weak_factory_{this};
};

// SyncNetworkChannel implements common tasks needed to interact with
// invalidation library:
//  - registering message and network status callbacks
//  - notifying observers about network channel state change
// Implementation of particular network protocol should implement
// SendMessage and call NotifyStateChange and DeliverIncomingMessage.
class INVALIDATION_EXPORT SyncNetworkChannel
    : public invalidation::NetworkChannel {
 public:
  class Observer {
   public:
    // Called when network channel state changes. Possible states are:
    //  - INVALIDATIONS_ENABLED : connection is established and working
    //  - TRANSIENT_INVALIDATION_ERROR : no network, connection lost, etc.
    //  - INVALIDATION_CREDENTIALS_REJECTED : Issues with auth token
    virtual void OnNetworkChannelStateChanged(
        InvalidatorState invalidator_state) = 0;
  };

  SyncNetworkChannel();

  ~SyncNetworkChannel() override;

  // invalidation::NetworkChannel implementation.
  // SyncNetworkChannel doesn't implement SendMessage. It is responsibility of
  // subclass to implement it.
  void SetMessageReceiver(
      invalidation::MessageCallback* incoming_receiver) override;
  void AddNetworkStatusReceiver(
      invalidation::NetworkStatusCallback* network_status_receiver) override;
  void SetSystemResources(invalidation::SystemResources* resources) override;

  // Subclass should implement UpdateCredentials to pass new token to channel
  // library.
  virtual void UpdateCredentials(const CoreAccountId& account_id,
                                 const std::string& token) = 0;

  // Return value from GetInvalidationClientType will be passed to
  // invalidation::CreateInvalidationClient. Subclass should return one of the
  // values from ipc::invalidation::ClientType enum from types.proto.
  virtual int GetInvalidationClientType() = 0;

  // Subclass should implement RequestDetailedStatus to provide debugging
  // information.
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) = 0;

  // Classes interested in network channel state changes should implement
  // SyncNetworkChannel::Observer and register here.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Helper functions that know how to construct network channels from channel
  // specific parameters.
  static std::unique_ptr<SyncNetworkChannel> CreatePushClientChannel(
      const notifier::NotifierOptions& notifier_options);
  static std::unique_ptr<SyncNetworkChannel> CreateGCMNetworkChannel(
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      network::NetworkConnectionTracker* network_connection_tracker,
      std::unique_ptr<GCMNetworkChannelDelegate> delegate);

  // Get the count of how many valid received messages were received.
  int GetReceivedMessagesCount() const;

 protected:
  // Subclass should call NotifyNetworkStatusChange to notify about network
  // changes. This triggers cacheinvalidation to try resending failed message
  // ahead of schedule when client comes online or IP address changes.
  void NotifyNetworkStatusChange(bool online);

  // Subclass should notify about connection state through
  // NotifyChannelStateChange. If communication doesn't work and it is possible
  // that invalidations from server will not reach this client then channel
  // should call this function with TRANSIENT_INVALIDATION_ERROR.
  void NotifyChannelStateChange(InvalidatorState invalidator_state);

  // Subclass should call DeliverIncomingMessage for message to reach
  // invalidations library.
  bool DeliverIncomingMessage(const std::string& message);

 private:
  // Callbacks into invalidation library
  std::unique_ptr<invalidation::MessageCallback> incoming_receiver_;
  std::vector<std::unique_ptr<invalidation::NetworkStatusCallback>>
      network_status_receivers_;

  // Last network status for new network status receivers.
  bool last_network_status_;

  int received_messages_count_;

  base::ObserverList<Observer>::Unchecked observers_;
};

class SyncStorage : public invalidation::Storage {
 public:
  SyncStorage(StateWriter* state_writer, invalidation::Scheduler* scheduler);

  ~SyncStorage() override;

  void SetInitialState(const std::string& value) {
    cached_state_ = value;
  }

  // invalidation::Storage implementation.
  void WriteKey(const std::string& key,
                const std::string& value,
                invalidation::WriteKeyCallback* done) override;

  void ReadKey(const std::string& key,
               invalidation::ReadKeyCallback* done) override;

  void DeleteKey(const std::string& key,
                 invalidation::DeleteKeyCallback* done) override;

  void ReadAllKeys(invalidation::ReadAllKeysCallback* key_callback) override;

  void SetSystemResources(invalidation::SystemResources* resources) override;

 private:
  // Runs the given storage callback with SUCCESS status and deletes it.
  void RunAndDeleteWriteKeyCallback(
      invalidation::WriteKeyCallback* callback);

  // Runs the given callback with the given value and deletes it.
  void RunAndDeleteReadKeyCallback(
      invalidation::ReadKeyCallback* callback, const std::string& value);

  StateWriter* state_writer_;
  invalidation::Scheduler* scheduler_;
  std::string cached_state_;
};

class INVALIDATION_EXPORT SyncSystemResources
    : public invalidation::SystemResources {
 public:
  SyncSystemResources(SyncNetworkChannel* sync_network_channel,
                      StateWriter* state_writer);

  ~SyncSystemResources() override;

  // invalidation::SystemResources implementation.
  void Start() override;
  void Stop() override;
  bool IsStarted() const override;
  virtual void set_platform(const std::string& platform);
  std::string platform() const override;
  SyncLogger* logger() override;
  SyncStorage* storage() override;
  SyncNetworkChannel* network() override;
  SyncInvalidationScheduler* internal_scheduler() override;
  SyncInvalidationScheduler* listener_scheduler() override;

 private:
  bool is_started_;
  std::string platform_;
  std::unique_ptr<SyncLogger> logger_;
  std::unique_ptr<SyncInvalidationScheduler> internal_scheduler_;
  std::unique_ptr<SyncInvalidationScheduler> listener_scheduler_;
  std::unique_ptr<SyncStorage> storage_;
  // sync_network_channel_ is owned by SyncInvalidationListener.
  SyncNetworkChannel* sync_network_channel_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_SYNC_SYSTEM_RESOURCES_H_
