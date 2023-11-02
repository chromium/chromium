// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"

namespace breadcrumbs {

BreadcrumbManagerObserver::BreadcrumbManagerObserver() {
  breadcrumb_manager_observation_.Observe(&BreadcrumbManager::GetInstance());
}

BreadcrumbManagerObserver::~BreadcrumbManagerObserver() = default;

}  // namespace breadcrumbs