// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_monitor.h"

#include "base/callback.h"
#include "base/notreached.h"

namespace {
class FuchsiaInstalledVersionMonitor : public InstalledVersionMonitor {
  void Start(Callback callback) override {
    // TODO(crbug.com/1318672)
    NOTIMPLEMENTED_LOG_ONCE();
  }
};
}  // namespace

// static
std::unique_ptr<InstalledVersionMonitor> InstalledVersionMonitor::Create() {
  // TODO(crbug.com/1318672)
  NOTIMPLEMENTED_LOG_ONCE();
  return std::make_unique<FuchsiaInstalledVersionMonitor>();
}
