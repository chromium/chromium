// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mgs/managed_guest_session_utils.h"

#include "chromeos/ash/components/login/login_state/login_state.h"

namespace chromeos {

bool IsManagedGuestSession() {
  return ash::LoginState::IsInitialized() &&
         ash::LoginState::Get()->IsManagedGuestSessionUser();
}

}  // namespace chromeos
