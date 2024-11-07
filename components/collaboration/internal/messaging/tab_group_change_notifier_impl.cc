// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace collaboration::messaging {

TabGroupChangeNotifierImpl::TabGroupChangeNotifierImpl(
    tab_groups::TabGroupSyncService* tab_group_sync_service)
    : tab_group_sync_service_(tab_group_sync_service) {}

TabGroupChangeNotifierImpl::~TabGroupChangeNotifierImpl() {
  if (has_tab_group_sync_service_observer_) {
    tab_group_sync_service_->RemoveObserver(this);
  }
}

void TabGroupChangeNotifierImpl::Initialize() {
  has_tab_group_sync_service_observer_ = true;
  tab_group_sync_service_->AddObserver(this);
}

void TabGroupChangeNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TabGroupChangeNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TabGroupChangeNotifierImpl::IsInitialized() {
  return is_initialized_;
}

void TabGroupChangeNotifierImpl::OnInitialized() {
  is_initialized_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TabGroupChangeNotifierImpl::NotifyTabGroupChangeNotifierInitialized,
          weak_ptr_factory_.GetWeakPtr()));
}

void TabGroupChangeNotifierImpl::NotifyTabGroupChangeNotifierInitialized()
    const {
  for (auto& observer : observers_) {
    observer.OnTabGroupChangeNotifierInitialized();
  }
}

}  // namespace collaboration::messaging
