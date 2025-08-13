// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_

#include "base/component_export.h"

namespace ash {

// A service that automatically signs out a user out of their previous
// session when they sign in to a new ChromeOS device.
// When a new sign-in is detected, an automatic sign-out is triggered which
// ensures that only one active session exists for a user at any given time.
class COMPONENT_EXPORT(AUTO_SIGN_OUT) AutoSignOutService {
 public:
  AutoSignOutService();
  AutoSignOutService(const AutoSignOutService&) = delete;
  AutoSignOutService& operator=(const AutoSignOutService&) = delete;

  ~AutoSignOutService();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTO_SIGN_OUT_AUTO_SIGN_OUT_SERVICE_H_
