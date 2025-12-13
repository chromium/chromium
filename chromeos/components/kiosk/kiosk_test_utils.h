// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_

#include <string_view>

namespace chromeos {

// Sets up a fake kiosk session for unit tests.
// Make sure to enable `UserManagerImpl` to be returned from
// `UserManager::Get()` prior to calling this function.
extern void SetUpFakeChromeAppKioskSession(
    std::string_view email = "example@kiosk-apps.device-local.localhost");

extern void SetUpFakeWebKioskSession(
    std::string_view email = "example@web-kiosk-apps.device-local.localhost");

extern void SetUpFakeIwaKioskSession(
    std::string_view email =
        "example@isolated-kiosk-apps.device-local.localhost");

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_
