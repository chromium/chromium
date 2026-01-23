// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_util.h"

#include "base/version.h"
#include "components/version_info/version_info.h"

namespace borealis {

int GetMilestone() {
  return version_info::GetVersion().components()[0];
}

}  // namespace borealis
