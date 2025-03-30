// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/browser_utils.h"

#include <string>

#include "net/base/network_interfaces.h"

namespace device_signals {

std::string GetHostName() {
  return net::GetHostName();
}

}  // namespace device_signals
