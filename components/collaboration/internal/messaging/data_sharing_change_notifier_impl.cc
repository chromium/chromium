// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier.h"
#include "components/data_sharing/public/data_sharing_service.h"

namespace collaboration::messaging {

DataSharingChangeNotifierImpl::DataSharingChangeNotifierImpl(
    data_sharing::DataSharingService* data_sharing_service)
    : data_sharing_service_(data_sharing_service) {}

DataSharingChangeNotifierImpl::~DataSharingChangeNotifierImpl() = default;

void DataSharingChangeNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (is_initialized_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DataSharingChangeNotifierImpl::
                           NotifyDataSharingChangeNotifierInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void DataSharingChangeNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DataSharingChangeNotifierImpl::Initialize() {
  data_sharing_service_observer_.Observe(data_sharing_service_);

  if (data_sharing_service_->IsGroupDataModelLoaded()) {
    is_initialized_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DataSharingChangeNotifierImpl::
                           NotifyDataSharingChangeNotifierInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void DataSharingChangeNotifierImpl::OnGroupDataModelLoaded() {
  if (is_initialized_) {
    // The DataSharingService was ready at startup, so we do not need to do
    // anything now.
    return;
  }

  is_initialized_ = true;

  // This is the first time we know about initialization, so inform our
  // observers. Since we are reacting to a callback, we do not need to post
  // this.
  NotifyDataSharingChangeNotifierInitialized();
}

bool DataSharingChangeNotifierImpl::IsInitialized() {
  return is_initialized_;
}

void DataSharingChangeNotifierImpl::NotifyDataSharingChangeNotifierInitialized()
    const {
  for (auto& observer : observers_) {
    observer.OnDataSharingChangeNotifierInitialized();
  }
}

}  // namespace collaboration::messaging
