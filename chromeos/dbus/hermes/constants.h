// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HERMES_CONSTANTS_H_
#define CHROMEOS_DBUS_HERMES_CONSTANTS_H_

#include "base/component_export.h"

namespace chromeos {
namespace hermes_constants {

// The timeout, in ms, to be used for D-Bus calls made to Hermes for operations
// which require network calls. This timeout is larger than the default D-Bus
// timeout since network operations can take an extended amount of time on a
// very slow connection or to a very slow back-end.
COMPONENT_EXPORT(HERMES_CLIENT) extern int kHermesNetworkOperationTimeoutMs;

}  // namespace hermes_constants
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HERMES_CONSTANTS_H_
