// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"

#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "components/crash/core/common/crash_key.h"

namespace breadcrumbs {

const char kBreadcrumbsProductDataKey[] = "breadcrumbs";

CrashReporterBreadcrumbObserver::CrashReporterBreadcrumbObserver() = default;
CrashReporterBreadcrumbObserver::~CrashReporterBreadcrumbObserver() = default;

CrashReporterBreadcrumbObserver&
CrashReporterBreadcrumbObserver::GetInstance() {
  static base::NoDestructor<CrashReporterBreadcrumbObserver> instance;
  return *instance;
}

void CrashReporterBreadcrumbObserver::SetPreviousSessionEvents(
    const std::vector<std::string>& events) {
  for (auto event_it = events.rbegin(); event_it != events.rend(); ++event_it) {
    breadcrumbs_ += *event_it;
    breadcrumbs_ += '\n';
  }
  UpdateBreadcrumbEventsCrashKey();
}

void CrashReporterBreadcrumbObserver::ObserveBreadcrumbManager(
    BreadcrumbManager* breadcrumb_manager) {
  breadcrumb_manager_observations_.AddObservation(breadcrumb_manager);
}

void CrashReporterBreadcrumbObserver::StopObservingBreadcrumbManager(
    BreadcrumbManager* breadcrumb_manager) {
  breadcrumb_manager_observations_.RemoveObservation(breadcrumb_manager);
}

void CrashReporterBreadcrumbObserver::ObserveBreadcrumbManagerService(
    BreadcrumbManagerKeyedService* breadcrumb_manager_service) {
  breadcrumb_manager_service_observations_.AddObservation(
      breadcrumb_manager_service);
}

void CrashReporterBreadcrumbObserver::StopObservingBreadcrumbManagerService(
    BreadcrumbManagerKeyedService* breadcrumb_manager_service) {
  // |breadcrumb_manager_service| may not be observed, because the incognito
  // BrowserState deletes itself before initializing on iOS (see
  // scene_controller::destroyAndRebuildIncognitoBrowserState).
  if (breadcrumb_manager_service_observations_.IsObservingSource(
          breadcrumb_manager_service)) {
    breadcrumb_manager_service_observations_.RemoveObservation(
        breadcrumb_manager_service);
  }
}

void CrashReporterBreadcrumbObserver::UpdateBreadcrumbEventsCrashKey() {
  if (breadcrumbs_.length() > kMaxDataLength)
    breadcrumbs_.resize(kMaxDataLength);
  static crash_reporter::CrashKeyString<kMaxDataLength> key(
      kBreadcrumbsProductDataKey);
  key.Set(breadcrumbs_);
}

void CrashReporterBreadcrumbObserver::EventAdded(BreadcrumbManager* manager,
                                                 const std::string& event) {
  std::string event_with_separator = event + '\n';
  breadcrumbs_.insert(0, event_with_separator);
  UpdateBreadcrumbEventsCrashKey();
}

}  // namespace breadcrumbs
