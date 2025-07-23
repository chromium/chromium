// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"

#include <numeric>
#include <string>

#include "base/containers/adapters.h"
#include "base/no_destructor.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "components/crash/core/common/crash_key.h"

namespace breadcrumbs {

namespace {

constexpr char kEventSeparator[] = "\n";

}  // namespace

CrashReporterBreadcrumbObserver::CrashReporterBreadcrumbObserver() = default;
CrashReporterBreadcrumbObserver::~CrashReporterBreadcrumbObserver() = default;

CrashReporterBreadcrumbObserver&
CrashReporterBreadcrumbObserver::GetInstance() {
  static base::NoDestructor<CrashReporterBreadcrumbObserver> instance;
  return *instance;
}

void CrashReporterBreadcrumbObserver::PreviousSessionEventsAdded() {
  UpdateBreadcrumbEventsCrashKey();
}

void CrashReporterBreadcrumbObserver::EventAdded(const std::string& event) {
  UpdateBreadcrumbEventsCrashKey();
}

void CrashReporterBreadcrumbObserver::UpdateBreadcrumbEventsCrashKey() {
  const auto& breadcrumbs = BreadcrumbManager::GetInstance().GetEvents();

  // Get the length of all breadcrumbs combined and preallocate the space needed
  // for the combined string. This saves repeated allocations in the next loop.
  const size_t event_separator_length = strlen(kEventSeparator);
  const size_t breadcrumbs_string_length = std::accumulate(
      breadcrumbs.begin(), breadcrumbs.end(), 0,
      [event_separator_length](const size_t sum,
                               const std::string& breadcrumb) {
        return sum + breadcrumb.length() + event_separator_length;
      });
  std::string breadcrumbs_string;
  breadcrumbs_string.reserve(breadcrumbs_string_length);

  // Concatenate breadcrumbs backwards, putting new breadcrumbs at the front, so
  // that the most relevant (i.e., newest) breadcrumbs are at the top in Crash.
  for (const std::string& breadcrumb : base::Reversed(breadcrumbs)) {
    breadcrumbs_string += breadcrumb;
    breadcrumbs_string += kEventSeparator;
  }
  DCHECK(breadcrumbs_string.length() == breadcrumbs_string_length);

  // Enforce a maximum length to ensure the string fits in the crash report;
  // this is unlikely to be needed due to the limit of `kMaxBreadcrumbs` events.
  if (breadcrumbs_string.length() > kMaxDataLength)
    breadcrumbs_string.resize(kMaxDataLength);

  static crash_reporter::CrashKeyString<kMaxDataLength> key(
      kBreadcrumbsProductDataKey);
  key.Set(breadcrumbs_string);
}

}  // namespace breadcrumbs
