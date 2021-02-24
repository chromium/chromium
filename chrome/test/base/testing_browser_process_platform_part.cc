// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_mac.h"
#include "services/device/public/cpp/test/fake_geolocation_system_permission.h"

TestingBrowserProcessPlatformPart::TestingBrowserProcessPlatformPart() {
#if defined(OS_MAC)
  location_permission_manager_ =
      std::make_unique<FakeSystemGeolocationPermissionsManager>();
#endif
}

TestingBrowserProcessPlatformPart::~TestingBrowserProcessPlatformPart() {
}
#if defined(OS_MAC)
void TestingBrowserProcessPlatformPart::SetLocationPermissionManager(
    std::unique_ptr<device::GeolocationSystemPermissionManager>
        location_permission_manager) {
  location_permission_manager_ = std::move(location_permission_manager);
}
#endif
