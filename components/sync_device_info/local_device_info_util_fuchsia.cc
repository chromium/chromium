// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/notreached.h"

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  // TODO(crbug.com/1233497): Request the name of the device from the system.
  NOTIMPLEMENTED_LOG_ONCE();
  return std::string("Fuchsia");
}

}  // namespace syncer
