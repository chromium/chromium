// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

#include "base/callback.h"
#include "base/notreached.h"

void GetInstalledVersion(InstalledVersionCallback callback) {
  // TODO(crbug.com/1235293)
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(InstalledAndCriticalVersion(base::Version()));
}
