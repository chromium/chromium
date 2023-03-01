// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process_platform_part.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#endif

TestingBrowserProcessPlatformPart::TestingBrowserProcessPlatformPart() {
#if BUILDFLAG(IS_MAC)
  auto fake_geolocation_manager =
      std::make_unique<device::FakeGeolocationManager>();
  fake_geolocation_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kAllowed);
  geolocation_manager_ = std::move(fake_geolocation_manager);
#endif
}

TestingBrowserProcessPlatformPart::~TestingBrowserProcessPlatformPart() {
}
#if BUILDFLAG(IS_MAC)
void TestingBrowserProcessPlatformPart::SetGeolocationManager(
    std::unique_ptr<device::GeolocationManager> geolocation_manager) {
  geolocation_manager_ = std::move(geolocation_manager);
}
#endif
