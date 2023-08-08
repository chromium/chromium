// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/constants.h"

namespace ash::hermes_constants {

// 6min * 60s/min * 1000ms/s.
int kHermesNetworkOperationTimeoutMs = 6 * 60 * 1000;

// 2min * 60s/min * 1000ms/s
int kHermesOperationTimeoutMs = 2 * 60 * 1000;

}  // namespace ash::hermes_constants
