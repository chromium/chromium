// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/sync_system_resources.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/gcm_network_channel.h"
#include "components/invalidation/impl/gcm_network_channel_delegate.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "components/invalidation/public/invalidation_util.h"
#include "google/cacheinvalidation/deps/callback.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/push_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

SyncLogger::SyncLogger() {}
SyncLogger::~SyncLogger() {}

void SyncLogger::Log(LogLevel level, const char* file, int line,
                     const char* format, ...) {
  logging::LogSeverity log_severity = -2;  // VLOG(2)
  bool emit_log = false;
  switch (level) {
    case FINE_LEVEL:
      log_severity = -2;  // VLOG(2)
      emit_log = VLOG_IS_ON(2);
      break;
    case INFO_LEVEL:
      log_severity = -1;  // VLOG(1)
      emit_log = VLOG_IS_ON(1);
      break;
    case WARNING_LEVEL:
      log_severity = logging::LOG_WARNING;
      emit_log = LOG_IS_ON(WARNING);
      break;
    case SEVERE_LEVEL:
      log_severity = logging::LOG_ERROR;
      emit_log = LOG_IS_ON(ERROR);
      break;
  }
  if (emit_log) {
    va_list ap;
    va_start(ap, format);
    std::string result;
    base::StringAppendV(&result, format, ap);
    logging::LogMessage(file, line, log_severity).stream() << result;
    va_end(ap);
  }
}

void SyncLogger::SetSystemResources(invalidation::SystemResources* resources) {
  // Do nothing.
}

SyncInvalidationScheduler::SyncInvalidationScheduler()
    : created_on_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      is_started_(false),
      is_stopped_(false) {
  CHECK(!!created_on_task_runner_);
}

SyncInvalidationScheduler::~SyncInvalidationScheduler() {
  CHECK(IsRunningOnThread());
  CHECK(is_stopped_);
}

void SyncInvalidationScheduler::Start() {
  CHECK(IsRunningOnThread());
  CHECK(!is_started_);
  is_started_ = true;
  is_stopped_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

void SyncInvalidationScheduler::Stop() {
  CHECK(IsRunningOnThread());
  is_stopped_ = true;
  is_started_ = false;
  weak_factory_.InvalidateWeakPtrs();
  posted_tasks_.clear();
}

void SyncInvalidationScheduler::Schedule(invalidation::TimeDelta delay,
                                         invalidation::Closure* task) {
  DCHECK(invalidation::IsCallbackRepeatable(task));
  CHECK(IsRunningOnThread());

  if (!is_started_) {
    delete task;
    return;
  }

  posted_tasks_.insert(base::WrapUnique(task));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SyncInvalidationScheduler::RunPostedTask,
                     weak_factory_.GetWeakPtr(), task),
      delay);
}

bool SyncInvalidationScheduler::IsRunningOnThread() const {
  return created_on_task_runner_->BelongsToCurrentThread();
}

invalidation::Time SyncInvalidationScheduler::GetCurrentTime() const {
  CHECK(IsRunningOnThread());
  return base::Time::Now();
}

void SyncInvalidationScheduler::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void SyncInvalidationScheduler::RunPostedTask(invalidation::Closure* task) {
  CHECK(IsRunningOnThread());
  task->Run();
  auto it = posted_tasks_.find(task);
  posted_tasks_.erase(it);
}

SyncNetworkChannel::SyncNetworkChannel()
    : last_network_status_(false),
      received_messages_count_(0) {}

SyncNetworkChannel::~SyncNetworkChannel() {
}

void SyncNetworkChannel::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  incoming_receiver_.reset(incoming_receiver);
}

void SyncNetworkChannel::AddNetworkStatusReceiver(
    invalidation::NetworkStatusCallback* network_status_receiver) {
  network_status_receiver->Run(last_network_status_);
  network_status_receivers_.push_back(
      base::WrapUnique(network_status_receiver));
}

void SyncNetworkChannel::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void SyncNetworkChannel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncNetworkChannel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<SyncNetworkChannel> SyncNetworkChannel::CreatePushClientChannel(
    const notifier::NotifierOptions& notifier_options) {
  std::unique_ptr<notifier::PushClient> push_client(
      notifier::PushClient::CreateDefaultOnIOThread(notifier_options));
  return std::make_unique<PushClientChannel>(std::move(push_client));
}

std::unique_ptr<SyncNetworkChannel> SyncNetworkChannel::CreateGCMNetworkChannel(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<GCMNetworkChannelDelegate> delegate) {
  DCHECK(url_loader_factory_info);
  return std::make_unique<GCMNetworkChannel>(
      network::SharedURLLoaderFactory::Create(
          std::move(url_loader_factory_info)),
      network_connection_tracker, std::move(delegate));
}

void SyncNetworkChannel::NotifyNetworkStatusChange(bool online) {
  // Remember network state for future NetworkStatusReceivers.
  last_network_status_ = online;
  // Notify NetworkStatusReceivers in cacheinvalidation.
  for (const auto& receiver : network_status_receivers_) {
    receiver->Run(online);
  }
}

void SyncNetworkChannel::NotifyChannelStateChange(
    InvalidatorState invalidator_state) {
  for (auto& observer : observers_)
    observer.OnNetworkChannelStateChanged(invalidator_state);
}

bool SyncNetworkChannel::DeliverIncomingMessage(const std::string& message) {
  if (!incoming_receiver_) {
    DLOG(ERROR) << "No receiver for incoming notification";
    return false;
  }
  received_messages_count_++;
  incoming_receiver_->Run(message);
  return true;
}

int SyncNetworkChannel::GetReceivedMessagesCount() const {
  return received_messages_count_;
}

SyncStorage::SyncStorage(StateWriter* state_writer,
                         invalidation::Scheduler* scheduler)
    : state_writer_(state_writer),
      scheduler_(scheduler) {
  DCHECK(state_writer_);
  DCHECK(scheduler_);
}

SyncStorage::~SyncStorage() {}

void SyncStorage::WriteKey(const std::string& key, const std::string& value,
                           invalidation::WriteKeyCallback* done) {
  CHECK(state_writer_);
  // TODO(ghc): actually write key,value associations, and don't invoke the
  // callback until the operation completes.
  state_writer_->WriteState(value);
  cached_state_ = value;
  // According to the cache invalidation API folks, we can do this as
  // long as we make sure to clear the persistent state that we start
  // up the cache invalidation client with.  However, we mustn't do it
  // right away, as we may be called under a lock that the callback
  // uses.
  scheduler_->Schedule(
      invalidation::Scheduler::NoDelay(),
      invalidation::NewPermanentCallback(
          this, &SyncStorage::RunAndDeleteWriteKeyCallback,
          done));
}

void SyncStorage::ReadKey(const std::string& key,
                          invalidation::ReadKeyCallback* done) {
  DCHECK(scheduler_->IsRunningOnThread()) << "not running on scheduler thread";
  RunAndDeleteReadKeyCallback(done, cached_state_);
}

void SyncStorage::DeleteKey(const std::string& key,
                            invalidation::DeleteKeyCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to DeleteKey(" << key << ", callback)";
}

void SyncStorage::ReadAllKeys(invalidation::ReadAllKeysCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to ReadAllKeys(callback)";
}

void SyncStorage::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void SyncStorage::RunAndDeleteWriteKeyCallback(
    invalidation::WriteKeyCallback* callback) {
  callback->Run(
      invalidation::Status(invalidation::Status::SUCCESS, std::string()));
  delete callback;
}

void SyncStorage::RunAndDeleteReadKeyCallback(
    invalidation::ReadKeyCallback* callback, const std::string& value) {
  callback->Run(std::make_pair(
      invalidation::Status(invalidation::Status::SUCCESS, std::string()),
      value));
  delete callback;
}

SyncSystemResources::SyncSystemResources(
    SyncNetworkChannel* sync_network_channel,
    StateWriter* state_writer)
    : is_started_(false),
      logger_(new SyncLogger()),
      internal_scheduler_(new SyncInvalidationScheduler()),
      listener_scheduler_(new SyncInvalidationScheduler()),
      storage_(state_writer
                   ? new SyncStorage(state_writer, internal_scheduler_.get())
                   : nullptr),
      sync_network_channel_(sync_network_channel) {}

SyncSystemResources::~SyncSystemResources() {
  Stop();
}

void SyncSystemResources::Start() {
  internal_scheduler_->Start();
  listener_scheduler_->Start();
  is_started_ = true;
}

void SyncSystemResources::Stop() {
  internal_scheduler_->Stop();
  listener_scheduler_->Stop();
}

bool SyncSystemResources::IsStarted() const {
  return is_started_;
}

void SyncSystemResources::set_platform(const std::string& platform) {
  platform_ = platform;
}

std::string SyncSystemResources::platform() const {
  return platform_;
}

SyncLogger* SyncSystemResources::logger() {
  return logger_.get();
}

SyncStorage* SyncSystemResources::storage() {
  return storage_ ? storage_.get() : nullptr;
}

SyncNetworkChannel* SyncSystemResources::network() {
  return sync_network_channel_;
}

SyncInvalidationScheduler* SyncSystemResources::internal_scheduler() {
  return internal_scheduler_.get();
}

SyncInvalidationScheduler* SyncSystemResources::listener_scheduler() {
  return listener_scheduler_.get();
}

}  // namespace syncer
