// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"

#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace breadcrumbs {

// Test fixture for testing BreadcrumbManagerKeyedService class.
typedef PlatformTest BreadcrumbManagerKeyedServiceTest;

// Tests that events logged to Normal and OffTheRecord BrowserStates are
// separately identifiable.
TEST_F(BreadcrumbManagerKeyedServiceTest, EventsLabeledWithBrowserState) {
  std::unique_ptr<BreadcrumbManagerKeyedService> breadcrumb_manager_service =
      std::make_unique<BreadcrumbManagerKeyedService>(
          /*is_off_the_record=*/false);
  breadcrumb_manager_service->AddEvent("event");

  const std::string event =
      BreadcrumbManager::GetInstance().GetEvents().front();

  BreadcrumbManager::GetInstance().ResetForTesting();

  std::unique_ptr<BreadcrumbManagerKeyedService>
      otr_breadcrumb_manager_service =
          std::make_unique<BreadcrumbManagerKeyedService>(
              /*is_off_the_record=*/true);
  otr_breadcrumb_manager_service->AddEvent("event");

  const std::string off_the_record_event =
      BreadcrumbManager::GetInstance().GetEvents().front();
  // Event should indicate it was logged from an off-the-record "Incognito"
  // browser state.
  EXPECT_NE(std::string::npos, off_the_record_event.find(" I "));

  EXPECT_STRNE(event.c_str(), off_the_record_event.c_str());
}

}  // namespace breadcrumbs
