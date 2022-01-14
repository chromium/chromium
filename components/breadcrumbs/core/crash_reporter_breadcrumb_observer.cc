// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"

#include <numeric>
#include <string>

#include "base/containers/adapters.h"
#include "base/no_destructor.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "components/crash/core/common/crash_key.h"

namespace breadcrumbs {

namespace {

constexpr char kEventSeparator[] = "\n";

// The maximum number of breadcrumbs to attach to a crash report.
constexpr int kMaxBreadcrumbs = 30;

}  // namespace

const char kBreadcrumbsProductDataKey[] = "breadcrumbs";

CrashReporterBreadcrumbObserver::CrashReporterBreadcrumbObserver() = default;
CrashReporterBreadcrumbObserver::~CrashReporterBreadcrumbObserver() = default;

CrashReporterBreadcrumbObserver&
CrashReporterBreadcrumbObserver::GetInstance() {
  static base::NoDestructor<CrashReporterBreadcrumbObserver> instance;
  return *instance;
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

void CrashReporterBreadcrumbObserver::SetPreviousSessionEvents(
    const std::vector<std::string>& events) {
  breadcrumbs_.insert(breadcrumbs_.begin(), events.begin(), events.end());
  UpdateBreadcrumbEventsCrashKey();
}

void CrashReporterBreadcrumbObserver::EventAdded(BreadcrumbManager* manager,
                                                 const std::string& event) {
  breadcrumbs_.push_back(event);
  UpdateBreadcrumbEventsCrashKey();
}

void CrashReporterBreadcrumbObserver::UpdateBreadcrumbEventsCrashKey() {
  // Remove the oldest events to remain below the maximum number of breadcrumbs.
  while (breadcrumbs_.size() > kMaxBreadcrumbs)
    breadcrumbs_.pop_front();

  // Get the length of all breadcrumbs combined and preallocate the space needed
  // for the combined string. This saves repeated allocations in the next loop.
  const size_t event_separator_length = strlen(kEventSeparator);
  const size_t breadcrumbs_string_length = std::accumulate(
      breadcrumbs_.begin(), breadcrumbs_.end(), 0,
      [event_separator_length](const size_t sum,
                               const std::string& breadcrumb) {
        return sum + breadcrumb.length() + event_separator_length;
      });
  std::string breadcrumbs_string;
  breadcrumbs_string.reserve(breadcrumbs_string_length);

  // Concatenate breadcrumbs backwards, putting new breadcrumbs at the front, so
  // that the most relevant (i.e., newest) breadcrumbs are at the top in Crash.
  for (const std::string& breadcrumb : base::Reversed(breadcrumbs_)) {
    breadcrumbs_string += breadcrumb;
    breadcrumbs_string += kEventSeparator;
  }
  DCHECK(breadcrumbs_string.length() == breadcrumbs_string_length);

  // Enforce a maximum length to ensure the string fits in the crash report;
  // this is unlikely to be needed due to the limit of |kMaxBreadcrumbs| events.
  if (breadcrumbs_string.length() > kMaxDataLength)
    breadcrumbs_string.resize(kMaxDataLength);

  static crash_reporter::CrashKeyString<kMaxDataLength> key(
      kBreadcrumbsProductDataKey);
  key.Set(breadcrumbs_string);
}

}  // namespace breadcrumbs
