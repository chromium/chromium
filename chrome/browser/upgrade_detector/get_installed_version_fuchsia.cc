// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

#include "base/callback.h"
#include "base/notreached.h"
#include "components/version_info/version_info.h"

void GetInstalledVersion(InstalledVersionCallback callback) {
  // TODO(crbug.com/1235293): Check to see if a different version has been
  // installed on the device and is awaiting a restart. For the time being,
  // unconditionally return the currently-running version.
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(
      InstalledAndCriticalVersion(version_info::GetVersion()));
}
