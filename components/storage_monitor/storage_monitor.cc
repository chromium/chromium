// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "components/storage_monitor/transient_device_ids.h"

namespace storage_monitor {

namespace {

StorageMonitor* g_storage_monitor = nullptr;

}  // namespace

StorageMonitor::Receiver::~Receiver() {
}

class StorageMonitor::ReceiverImpl : public StorageMonitor::Receiver {
 public:
  explicit ReceiverImpl(StorageMonitor* notifications)
      : notifications_(notifications) {}

  ~ReceiverImpl() override {}

  void ProcessAttach(const StorageInfo& info) override;

  void ProcessDetach(const std::string& id) override;

  void MarkInitialized() override;

 private:
  StorageMonitor* notifications_;
};

void StorageMonitor::ReceiverImpl::ProcessAttach(const StorageInfo& info) {
  notifications_->ProcessAttach(info);
}

void StorageMonitor::ReceiverImpl::ProcessDetach(const std::string& id) {
  notifications_->ProcessDetach(id);
}

void StorageMonitor::ReceiverImpl::MarkInitialized() {
  notifications_->MarkInitialized();
}

// static
void StorageMonitor::Create(
    std::unique_ptr<service_manager::Connector> connector) {
  delete g_storage_monitor;
  g_storage_monitor = CreateInternal();
  g_storage_monitor->connector_ = std::move(connector);
}

service_manager::Connector* StorageMonitor::GetConnector() {
  return connector_.get();
}

// static
void StorageMonitor::Destroy() {
  delete g_storage_monitor;
  g_storage_monitor = nullptr;
}

StorageMonitor* StorageMonitor::GetInstance() {
  return g_storage_monitor;
}

void StorageMonitor::SetStorageMonitorForTesting(
    std::unique_ptr<StorageMonitor> storage_monitor) {
  delete g_storage_monitor;
  g_storage_monitor = storage_monitor.release();
}

std::vector<StorageInfo> StorageMonitor::GetAllAvailableStorages() const {
  std::vector<StorageInfo> results;

  base::AutoLock lock(storage_lock_);
  for (auto it = storage_map_.begin(); it != storage_map_.end(); ++it) {
    results.push_back(it->second);
  }
  return results;
}

void StorageMonitor::EnsureInitialized(base::Closure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialized_) {
    if (!callback.is_null())
      callback.Run();
    return;
  }

  if (!callback.is_null()) {
    on_initialize_callbacks_.push_back(callback);
  }

  if (initializing_)
    return;

  initializing_ = true;
  Init();
}

bool StorageMonitor::IsInitialized() const {
  return initialized_;
}

void StorageMonitor::AddObserver(RemovableStorageObserver* obs) {
  observer_list_->AddObserver(obs);
}

void StorageMonitor::RemoveObserver(
    RemovableStorageObserver* obs) {
  observer_list_->RemoveObserver(obs);
}

std::string StorageMonitor::GetTransientIdForDeviceId(
    const std::string& device_id) {
  return transient_device_ids_->GetTransientIdForDeviceId(device_id);
}

std::string StorageMonitor::GetDeviceIdForTransientId(
    const std::string& transient_id) const {
  return transient_device_ids_->DeviceIdFromTransientId(transient_id);
}

void StorageMonitor::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  // Platform-specific implementations will override this method to
  // perform actual device ejection.
  callback.Run(EJECT_FAILURE);
}

StorageMonitor::StorageMonitor()
    : observer_list_(
          new base::ObserverListThreadSafe<RemovableStorageObserver>()),
      initializing_(false),
      initialized_(false),
      transient_device_ids_(new TransientDeviceIds) {
  receiver_.reset(new ReceiverImpl(this));
}

StorageMonitor::~StorageMonitor() {
}

StorageMonitor::Receiver* StorageMonitor::receiver() const {
  return receiver_.get();
}

void StorageMonitor::MarkInitialized() {
  initialized_ = true;
  for (auto iter = on_initialize_callbacks_.begin();
       iter != on_initialize_callbacks_.end(); ++iter) {
    iter->Run();
  }
  on_initialize_callbacks_.clear();
}

void StorageMonitor::ProcessAttach(const StorageInfo& info) {
  {
    base::AutoLock lock(storage_lock_);
    if (base::Contains(storage_map_, info.device_id())) {
      // This can happen if our unique id scheme fails. Ignore the incoming
      // non-unique attachment.
      return;
    }
    storage_map_.insert(std::make_pair(info.device_id(), info));
  }

  DVLOG(1) << "StorageAttached id " << info.device_id();
  if (StorageInfo::IsRemovableDevice(info.device_id())) {
    observer_list_->Notify(
        FROM_HERE, &RemovableStorageObserver::OnRemovableStorageAttached, info);
  }
}

void StorageMonitor::ProcessDetach(const std::string& id) {
  StorageInfo info;
  {
    base::AutoLock lock(storage_lock_);
    auto it = storage_map_.find(id);
    if (it == storage_map_.end())
      return;
    info = it->second;
    storage_map_.erase(it);
  }

  DVLOG(1) << "StorageDetached for id " << id;
  if (StorageInfo::IsRemovableDevice(info.device_id())) {
    observer_list_->Notify(
        FROM_HERE, &RemovableStorageObserver::OnRemovableStorageDetached, info);
  }
}

}  // namespace storage_monitor
