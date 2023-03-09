// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"

#include "base/strings/stringprintf.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"

namespace breadcrumbs {

void BreadcrumbManagerKeyedService::AddEvent(const std::string& event) {
  std::string event_log =
      base::StringPrintf("%s%s", browsing_mode_.c_str(), event.c_str());
  BreadcrumbManager::GetInstance().AddEvent(event_log);
}

BreadcrumbManagerKeyedService::BreadcrumbManagerKeyedService(
    bool is_off_the_record)
    // Set "I" for Incognito (Chrome branded OffTheRecord implementation) and
    // empty string for Normal browsing mode.
    : browsing_mode_(is_off_the_record ? "I " : "") {}

BreadcrumbManagerKeyedService::~BreadcrumbManagerKeyedService() = default;

}  // namespace breadcrumbs
