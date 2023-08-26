// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KIOSK_KIOSK_UTILS_H_
#define CHROMEOS_COMPONENTS_KIOSK_KIOSK_UTILS_H_

namespace chromeos {

// Returns true if a kiosk session is currently running.
extern bool IsKioskSession();

// Returns true if a web app (PWA) kiosk is currently running.
extern bool IsWebKioskSession();

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_KIOSK_KIOSK_UTILS_H_
