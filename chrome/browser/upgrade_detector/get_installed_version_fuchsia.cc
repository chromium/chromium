// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include "base/notreached.h"

InstalledAndCriticalVersion GetInstalledVersion() {
  // TODO(crbug.com/1235293)
  NOTIMPLEMENTED_LOG_ONCE();
  return InstalledAndCriticalVersion(base::Version());
}
