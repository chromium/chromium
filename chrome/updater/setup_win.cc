// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/setup.h"
#include "chrome/updater/win/setup/setup.h"

namespace updater {

int InstallCandidate(bool is_machine) {
  return Setup(is_machine);
}

}  // namespace updater
