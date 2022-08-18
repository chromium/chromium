// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"

#include "base/strings/stringprintf.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"
#include "components/breadcrumbs/core/breadcrumb_util.h"

namespace breadcrumbs {

void BreadcrumbManagerKeyedService::AddEvent(const std::string& event) {
  std::string event_log =
      base::StringPrintf("%s%s", browsing_mode_.c_str(), event.c_str());
  breadcrumb_manager_->AddEvent(event_log);
}

void BreadcrumbManagerKeyedService::AddObserver(
    BreadcrumbManagerObserver* observer) {
  breadcrumb_manager_->AddObserver(observer);
}

void BreadcrumbManagerKeyedService::RemoveObserver(
    BreadcrumbManagerObserver* observer) {
  breadcrumb_manager_->RemoveObserver(observer);
}

const std::list<std::string> BreadcrumbManagerKeyedService::GetEvents(
    size_t event_count_limit) const {
  return breadcrumb_manager_->GetEvents(event_count_limit);
}

void BreadcrumbManagerKeyedService::StartPersisting(
    BreadcrumbPersistentStorageManager* persistent_storage_manager) {
  DCHECK(persistent_storage_manager);

  if (persistent_storage_manager_)
    StopPersisting();

  persistent_storage_manager_ = persistent_storage_manager;
  persistent_storage_manager_->MonitorBreadcrumbManagerService(this);
}

void BreadcrumbManagerKeyedService::StopPersisting() {
  if (!persistent_storage_manager_)
    return;

  persistent_storage_manager_->StopMonitoringBreadcrumbManagerService(this);
  persistent_storage_manager_ = nullptr;
}

BreadcrumbPersistentStorageManager*
BreadcrumbManagerKeyedService::GetPersistentStorageManager() {
  return persistent_storage_manager_;
}

BreadcrumbManagerKeyedService::BreadcrumbManagerKeyedService(
    bool is_off_the_record)
    // Set "I" for Incognito (Chrome branded OffTheRecord implementation) and
    // empty string for Normal browsing mode.
    : browsing_mode_(is_off_the_record ? "I " : ""),
      breadcrumb_manager_(std::make_unique<BreadcrumbManager>(GetStartTime())) {
}

BreadcrumbManagerKeyedService::~BreadcrumbManagerKeyedService() = default;

}  // namespace breadcrumbs
