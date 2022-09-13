// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // for HOST_NAME_MAX
#include <unistd.h>  // for gethostname()

#include <string>

#include "base/linux_util.h"

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, HOST_NAME_MAX) == 0) {  // Success.
    return hostname;
  }
  return base::GetLinuxDistro();
}

}  // namespace syncer
