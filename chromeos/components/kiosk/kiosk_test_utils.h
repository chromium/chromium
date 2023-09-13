// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_

namespace chromeos {

// Sets up a fake kiosk session for unit tests.
// Make sure to enable `UserManagerBase` to be returned from
// `UserManager::Get()` prior to calling this function.
extern void SetUpFakeKioskSession();

// Tears down a fake kiosk session for unit tests.
extern void TearDownFakeKioskSession();

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_KIOSK_KIOSK_TEST_UTILS_H_
