// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_

#include <string>

namespace chromeos {

// Sets up a fake kiosk session for unit tests.
// Make sure to enable `UserManagerImpl` to be returned from
// `UserManager::Get()` prior to calling this function.
// TODO(b/40286020): remove the default parameter. That is only for transition
// purpose.
extern void SetUpFakeKioskSession(
    const std::string& email = "example@example.com");

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_
