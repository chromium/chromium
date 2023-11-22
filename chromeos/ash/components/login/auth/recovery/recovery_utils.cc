// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/recovery/recovery_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

bool GetRecoveryDefaultState(bool is_managed, PrefService* prefs) {
  if (is_managed) {
    return prefs->GetBoolean(ash::prefs::kRecoveryFactorBehavior);
  }

  // For non-managed users the default state is "disabled".
  return false;
}

}  // namespace ash
