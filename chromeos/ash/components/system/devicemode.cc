// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/devicemode.h"

#include "base/system/sys_info.h"

namespace chromeos {

// TODO(crbug.com/1408913): Remove this function and replace by
// IsRunningOnChromeOS().
bool IsRunningAsSystemCompositor() {
  return base::SysInfo::IsRunningOnChromeOS();
}

}  // namespace chromeos
