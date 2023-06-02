// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/command_line_switches.h"

#include <string>

#include "base/command_line.h"

namespace syncer {

bool IsSyncAllowedByFlag() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableSync);
}

}  // namespace syncer
