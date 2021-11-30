// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_nonsfi_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/nacl/common/nacl_switches.h"

namespace nacl {

// TODO(crbug.com/1273132): Remove.
bool IsNonSFIModeEnabled() {
  return false;
}

}  // namespace nacl
