// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_CONSTANTS_H_

#include "base/functional/callback_forward.h"
namespace ash::boca {

// Enum containing the states we expect from `remoting::RemotingClient`
enum class CrdConnectionState {
  kUnknown = 0,
  kConnecting = 1,
  kConnected = 2,
  kDisconnected = 3,
  kFailed = 4,
  kTimeout = 5
};

using SpotlightCrdStateUpdatedCallback =
    base::RepeatingCallback<void(CrdConnectionState)>;

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_CONSTANTS_H_
